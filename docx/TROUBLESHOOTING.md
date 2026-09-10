# 排障记录（Troubleshooting）

本文档记录本项目在开发过程中遇到的、根因不直观、值得沉淀下来的问题。每条记录包含：现象、排查过程、根本原因、解决方案、验证方法。目的是让后来的人（包括未来的自己）遇到类似现象时能快速定位，而不用重新走一遍排查过程。

---

## 问题一：LED 闪烁卡死在 `HAL_Delay()` 里出不来

### 现象

在 `core/src/main.c` 的主循环里用 `HAL_GPIO_TogglePin()` + `HAL_Delay(500)` 做闪灯，实测发现程序卡在 `HAL_Delay()` 的等待循环里，永远出不来，LED 完全不闪。用调试器打断点观察，`uwTick`（HAL 的毫秒计数变量）的值一直不变。

### 排查过程

先排查了一遍时钟/中断配置相关的代码，确认都是标准写法，没有问题：

- `HAL_Init()` → `HAL_InitTick()` → 配置 SysTick 每 1ms 产生一次中断；
- `SystemClock_Config()` 把系统时钟切到 180MHz 后，`HAL_RCC_ClockConfig()` 内部会自动重新调用一次 `HAL_InitTick()`，按新的时钟频率重新配置 SysTick（`drivers/src/stm32f4xx_hal_rcc.c` 里能看到这一行）；
- `core/src/stm32f4xx_it.c` 里的 `SysTick_Handler()` 正确调用了 `HAL_IncTick()`；
- `core/src/startup_stm32f429xx.S` 的向量表里 `SysTick_Handler` 也在正确的位置（第 15 个向量）。

代码逻辑上找不到问题，于是直接看编译出来的 ELF 文件，用 `arm-none-eabi-nm`/`objdump` 验证运行时实际发生了什么：

```bash
arm-none-eabi-nm build/stm32f429_firmware.elf | grep -Ei "SysTick_Handler|Default_Handler"
```

结果：

```
080003f0 T Default_Handler
080003f0 W SysTick_Handler        <-- 和 Default_Handler 完全一样的地址！
```

反汇编这个地址：

```
080003f0 <ADC_IRQHandler>:
 80003f0:  e7fe    b.n  80003f0     <-- 自己跳自己，死循环
```

结论：`SysTick_Handler` 从来没有真正执行过 `HAL_IncTick()`，而是一直在执行空的 `Default_Handler`（一条无限自跳转指令）。中断确实每 1ms 触发了一次，但触发之后 CPU 就永远陷在这个死循环里，主程序（包括 `HAL_Delay` 所在的 `main()`）再也没有机会往下走——这正好解释了"卡在 `HAL_Delay` 里"的现象。

### 根本原因

`core/src/startup_stm32f429xx.S`（启动文件）用 GNU 汇编的 `.thumb_set` 给向量表里**所有**没有专门实现的中断都定义了一个弱别名（weak alias），指向 `Default_Handler`：

```asm
.weak      SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler
```

这个弱别名和向量表本身（`g_pfnVectors`）是在**同一个目标文件** `startup_stm32f429xx.o` 里定义的。

而 `core/src/stm32f4xx_it.c` 里有真正的强定义：

```c
void SysTick_Handler(void)
{
  HAL_IncTick();
}
```

问题出在 `core` 在 CMake 里是一个 **STATIC 库**（`add_library(core STATIC)`）。链接器从静态库里抽取（extract）成员目标文件的规则是：**只有当前还存在"未决的未定义符号"时，才会去库里找能满足它的目标文件**。

链接执行到这里的实际情况是：

1. 链接器需要 `Reset_Handler`（程序入口），于是把 `startup_stm32f429xx.o` 从 `core.a` 里拉了出来；
2. `startup_stm32f429xx.o` 里同时还带着向量表 `g_pfnVectors`，以及它自己对 `SysTick_Handler` 等符号的（弱）定义；
3. 向量表对 `SysTick_Handler` 的引用，**在 `startup_stm32f429xx.o` 被拉进来的那一刻，就已经被它自己的弱别名满足了**；
4. 链接器没有任何理由再去 `core.a` 里翻找 `stm32f4xx_it.o`——因为从链接器的角度看，`SysTick_Handler` 已经"有定义了"（哪怕只是弱定义），没有未决符号需要继续搜索；
5. 于是 `stm32f4xx_it.o` 里那份真正调用 `HAL_IncTick()` 的强定义，永远没有机会参与链接。

这类问题只有在把启动文件和中断服务函数拆到**不同的编译单元、又打包进同一个静态库**时才会出现。ST 官方的 Makefile/CubeIDE 模板都是把所有 `.o` 文件直接摆在链接命令行上（不经过静态库打包），这种情况下链接器会无条件链入每一个 `.o`，不会触发这个坑。而本项目为了模块化，把 `core` 做成了独立的 STATIC 库，才把这个隐藏的坑挖了出来。

### 解决方案

在根目录 `CMakeLists.txt` 链接可执行文件的地方，把 `core` 用 `--whole-archive`/`--no-whole-archive` 包起来，强制链接器把这个静态库里的**每一个**成员目标文件都链入，而不是"按需抽取"：

```cmake
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    stm32_mcu_flags
    app
    threadx
    drivers
    -Wl,--whole-archive
    bsp
    core
    -Wl,--no-whole-archive
)
```

`--whole-archive` 之后，`stm32f4xx_it.o` 也会被完整链入，这样 ELF 链接器"强符号覆盖弱符号"的规则才能真正生效——`stm32f4xx_it.c` 里的强定义会覆盖 `startup_stm32f429xx.S` 里的弱别名。

### 验证方法

修复后重新编译，再执行一次同样的检查命令，两个符号地址应该不同，且反汇编 `SysTick_Handler` 应该能看到调用 `HAL_IncTick`：

```bash
arm-none-eabi-nm build/stm32f429_firmware.elf | grep -Ei "SysTick_Handler|HAL_IncTick"
# 080003f0 T Default_Handler
# 0800044e T SysTick_Handler      <-- 现在是独立地址了
# 08000580 W HAL_IncTick

arm-none-eabi-objdump -d build/stm32f429_firmware.elf | grep -A4 "<SysTick_Handler>:"
# 0800044e <SysTick_Handler>:
#  800044e: push {r7, lr}
#  8000450: add  r7, sp, #0
#  8000452: bl   8000580 <HAL_IncTick>   <-- 确认调到了 HAL_IncTick
```

之后再看向量表里 SysTick 那一项（偏移 `0x3C`）的值，也应该等于 `SysTick_Handler` 的地址（加 1，因为 Thumb 指令地址最低位置 1），而不是 `Default_Handler` 的地址。

### 举一反三：以后碰到"中断/回调好像没生效"该怎么排查

以后只要遇到"某个中断处理函数/回调函数看起来没有被真正调用"（比如中断确实触发了、但里面该做的事没发生），而代码逻辑又反复确认没问题时，可以怀疑是不是踩了同样的坑，用下面的方法确认：

```bash
arm-none-eabi-nm build/<固件>.elf | grep <怀疑没生效的函数名>
```

如果这个符号的类型是 `W`（weak）而不是 `T`（strong text）或 `t`，说明链接器最终选用的是弱定义，你写的那份强定义大概率没有被链入——去检查一下这两份定义是不是分别在同一个 STATIC 库的不同目标文件里，而"占坑"的那个弱定义所在的目标文件，是不是因为别的原因（比如向量表、比如同文件内部调用）已经被更早地、独立地拉进了链接。

---

## 问题二：移植串口打印功能后编译报错

### 背景

从之前的项目移植了一套基于 RT-Thread 的串口打印功能到 `bsp/` 目录下：

- `bsp/inc/bsp_uart.h` / `bsp/src/bsp_uart.c` —— USART1 的初始化和收发；
- `bsp/inc/rtthread.h` —— RT-Thread 的基础类型定义和 `rt_kprintf` 系列函数声明；
- `bsp/src/kservice.c` —— RT-Thread 自带的一套精简版内存/字符串/格式化函数（其中包含一份**精简版** `rt_vsnprintf`，标记为 `rt_weak`，不支持浮点格式）；
- `bsp/src/rt_vsnprintf.c` —— 另外单独移植的一份**完整版** `rt_vsnprintf`（文件头注释写着"porting for rt_vsnprintf as the fully functional version"），支持 `%f`/`%e`/`%g` 等浮点格式。

`core/src/main.c` 里新增了：

```c
#include "rtthread.h"
#include "bsp_uart.h"
...
my_uart_init(115200);
rt_kprintf("Hello RT-Thread:%ld \r\n", serial1);
rt_kprintf("Test Float:%lf \r\n", serial2);
```

### 现象与排查

直接执行 `cmake --build build`，编译在 `bsp` 库这一步连续报了三个错误，逐一排查：

#### 错误 1、2：`fatal error: uart.h: No such file or directory`

```
bsp/src/kservice.c:3:10: fatal error: uart.h: No such file or directory
bsp/src/rt_vsnprintf.c:55:10: fatal error: uart.h: No such file or directory
```

**原因**：这两个文件都写的是 `#include "uart.h"`，但项目里实际的头文件叫 `bsp_uart.h`（在旧项目里大概率就叫 `uart.h`，移植过来的时候文件改名了，但代码里的 `#include` 忘了同步改）。

**修复**：把两处 `#include "uart.h"` 都改成 `#include "bsp_uart.h"`。

#### 错误 3：`error: 'GPIO_SPEED_FAST' undeclared`

```
bsp/src/bsp_uart.c:42:36: error: 'GPIO_SPEED_FAST' undeclared (first use in this function);
did you mean 'I2C_SPEED_FAST'?
```

**原因**：`bsp_uart.c` 里的 `HAL_UART_MspInit()` 在配置 PA9 引脚速度时用了 `GPIO_SPEED_FAST`。这个宏名属于旧版本 HAL（或别的系列）的命名习惯，本项目当前 vendored 的 STM32F4xx HAL（`drivers/inc/stm32f4xx_hal_gpio.h`）里，速度档位的宏名是：

```c
GPIO_SPEED_FREQ_LOW
GPIO_SPEED_FREQ_MEDIUM
GPIO_SPEED_FREQ_HIGH
GPIO_SPEED_FREQ_VERY_HIGH
```

同一个文件里另一个函数 `my_uart_init()` 配置同样的 PA9/PA10 引脚时，用的就是正确的 `GPIO_SPEED_FREQ_HIGH`——说明 `HAL_UART_MspInit()` 那一处是移植/重复编写时漏改的。

**修复**：把 `HAL_UART_MspInit()` 里的 `GPIO_SPEED_FAST` 改成 `GPIO_SPEED_FREQ_HIGH`。

三处修完后 `cmake --build build` 可以完整跑完，生成 `.elf/.hex/.bin`。

### 隐藏的第二个问题：`rt_kprintf("%lf", ...)` 打不出浮点数

编译能过之后，鉴于本项目已经踩过一次"静态库里弱/强符号被链接器悄悄选错"的坑（见上面**问题一**），顺手用同样的方法复查了一下移植过来的 `rt_vsnprintf`，结果发现又中了一次：

```bash
arm-none-eabi-ar t build/bsp/libbsp.a
# bsp_led.c.obj
# bsp_uart.c.obj
# kservice.c.obj
# rt_vsnprintf.c.obj      <-- 完整版的目标文件确实被编译并打进了静态库

arm-none-eabi-nm build/stm32f429_firmware.elf | grep -i "rt_vsnprintf\b"
# 08000be8 W rt_vsnprintf   <-- 但最终链接进固件的是弱符号版本！

arm-none-eabi-nm build/stm32f429_firmware.elf | grep -i "print_floating_point"
# (什么都没有)              <-- 完整版专用的浮点格式化函数根本没进最终固件
```

**根因**：和问题一完全同构。`bsp` 也是一个 STATIC 库。`kservice.c` 里的 `rt_kprintf()` 内部会调用同文件里自己定义的那份 `rt_vsnprintf`（`rt_weak` 修饰，不支持 `%f`）。`kservice.o` 因为 `rt_kprintf` 被 `main.c` 用到而被拉入链接的那一刻，它内部对 `rt_vsnprintf` 的调用就已经被自己文件里的弱定义满足了，链接器根本没有理由再去同一个 `bsp.a` 里找 `rt_vsnprintf.c.obj` 里那份支持浮点的强定义。

**实际影响**：`kservice.c` 里那份精简版 `rt_vsnprintf` 的格式符解析（`switch (*fmt)`）根本没有 `'f'`/`'F'`/`'e'`/`'g'` 分支，遇到 `%lf` 会落进 `default` 分支，把 `%` 和 `f` 两个字符原样输出、且不消费对应的 `double` 参数。也就是说：

```c
rt_kprintf("Test Float:%lf \r\n", serial2);
```

实际打印出来的是字面的 `"Test Float:%lf"`，浮点数完全打不出来，而不是报错——这种"不报错但结果不对"的问题比编译错误更难发现。

**修复**：沿用问题一的方案，把 `bsp` 也纳入根 `CMakeLists.txt` 里 `--whole-archive` 的范围：

```cmake
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    stm32_mcu_flags
    app
    threadx
    drivers
    -Wl,--whole-archive
    bsp
    core
    -Wl,--no-whole-archive
)
```

### 验证方法

修复后重新编译，再跑一遍前面的检查命令：

```bash
arm-none-eabi-nm build/stm32f429_firmware.elf | grep -i "rt_vsnprintf\b|print_floating_point"
# 080023e0 t print_floating_point   <-- 完整版的内部函数出现了
# 08002fcc T rt_vsnprintf           <-- 变成强符号（T），且地址和之前不一样
```

固件体积（`.text` 段）从修复前的约 14.5KB 涨到约 22.9KB，也印证了完整版 `rt_vsnprintf`（连同它内部的浮点转换逻辑）确实被完整链入了。

### 遗留的非阻塞项（未处理，供参考）

- `bsp_uart.c` 里还留着一个没被任何地方调用的 `uart_init()`（`main.c` 实际调用的是功能重复的 `my_uart_init()`），是移植过程中留下的死代码，不影响编译和运行。
- 编译时有两条无害 warning：
  - `kservice.c` 的 `rt_vsnprintf` 里 `case 'X':` fallthrough 到 `case 'x':`，是故意的写法（`-Wimplicit-fallthrough`）；
  - `rt_vsnprintf.c` 里 `rtthread.h` 重新定义了 `va_start`，和 `<stdarg.h>` 已有的定义冲突（`rtthread.h` 在没有定义 `RT_USING_LIBC` 时会自己实现一套 `va_list`/`va_start`），两边最终都是同一个 GCC 内建实现，实际无害，但如果以后要清理告警，可以考虑让 `rtthread.h` 在检测到 `<stdarg.h>` 已经被包含时不要重复定义。

---

## 问题三：串口打印数据乱码

### 现象

串口初始化（`my_uart_init(115200)`）和 `rt_kprintf` 都已经能正常编译链接（见问题二），USB-TTL 接到 PC 上，串口终端设置成 115200 8N1（和代码里 `huart1.Init.BaudRate=115200`、`WordLength=UART_WORDLENGTH_8B`、`Parity=UART_PARITY_NONE`、`StopBits=UART_STOPBITS_1` 完全一致），但收到的数据是完全随机的乱码，没有任何规律。

### 排查过程

先排除了两类最容易想到的原因：

- **链接期弱/强符号覆盖问题**（问题一、问题二踩过的坑）：用 `arm-none-eabi-nm`/`objdump` 验证 `HAL_UART_MspInit` 确实链接成了 `bsp_uart.c` 里真正配置 GPIO/RCC 的强版本，不是 HAL 库里那个空的弱默认实现，排除。
- **终端参数不匹配**：确认过终端侧波特率/数据位/校验位/停止位都和代码一致，排除。

串口"乱码"（而不是完全收不到数据）是很典型的**波特率系统性偏差**的表现：PC 和 MCU 对每个 bit 的采样时刻错位，导致收到的字节值完全对不上，但通信本身（起始位、电平）是正常的。

USART 的波特率分频值是 HAL 在 `HAL_UART_Init()` 里根据 `HAL_RCC_GetPCLK2Freq()` 现场算出来的，而这个函数最终依赖的是 `core/inc/stm32f4xx_hal_conf.h` 里的编译期宏 `HSE_VALUE`——软件没有办法在运行时"测量"外部晶振的真实频率，只能相信这个宏里写的值。

进一步检查 `core/src/main.c` 的 `SystemClock_Config()`：

```c
RCC_OscInitStructure.PLL.PLLM = 25;   // 主 PLL 分频系数
RCC_OscInitStructure.PLL.PLLN = 360;
RCC_OscInitStructure.PLL.PLLP = 2;
```

`PLLM = 25` 这个取值本身就很反常——PLL 输入频率的标准做法是把 HSE 分频到 1MHz（这样 VCO 倍频系数、环路稳定性等参数手册上的推荐值才对得上）。如果 HSE 真的是模板默认注释写的 8MHz，`PLLM` 应该配成 `8`（8MHz / 8 = 1MHz）才对；而 `PLLM = 25` 恰好只有在 HSE = **25MHz** 时才能让 PLL 输入落在 1MHz（25MHz / 25 = 1MHz）。也就是说，这份 `SystemClock_Config()` 的 PLL 参数从一开始就是照着 25MHz 晶振配的，只是 `stm32f4xx_hal_conf.h` 里的 `HSE_VALUE` 宏没有跟着改，还留着 ST 官方模板的默认值 `8000000U`。

### 根本原因

`core/inc/stm32f4xx_hal_conf.h` 里：

```c
#define HSE_VALUE    (8000000U)
```

这个值和板子上真实焊接的 25MHz 晶振不匹配。`HAL_RCC_GetSysClockFreq()`（进而 `SystemCoreClock`、`HAL_RCC_GetPCLK2Freq()`）都是拿这个宏参与计算的：

- 软件"以为"的 SYSCLK = `(HSE_VALUE / PLLM) * PLLN / PLLP` = `(8MHz / 25) * 360 / 2` ≈ 57.6MHz；
- 硬件实际跑的 SYSCLK（因为真实晶振是 25MHz）= `(25MHz / 25) * 360 / 2` = 180MHz。

两者相差整整 `180 / 57.6 = 3.125` 倍。USART 的波特率分频值是按"软件以为的" 57.6MHz 对应的 PCLK2 算出来的，而 UART 外设实际是按真实的 180MHz 对应时钟在收发——分频系数和实际时钟不匹配，收发双方对每个 bit 的采样时刻完全对不上，表现为收到的字节是随机乱码。

（这个 3.125 倍的偏差同时也会影响 `HAL_Delay()`：`HAL_InitTick()` 配置 SysTick 重装载值时用的也是这个偏低的 `SystemCoreClock`，实际 1ms 节拍会被压缩成约 0.32ms，`HAL_Delay(500)` 实际只会延时约 160ms——只是这个偏差不像串口乱码那么容易一眼看出来，闪灯节奏"看起来快了一点"很容易被忽略过去。）

### 解决方案

把 `HSE_VALUE` 改成板子上真实的晶振频率：

```c
#if !defined  (HSE_VALUE)
  #define HSE_VALUE    (25000000U) /*!< 这块板子的 HSE 晶振是 25MHz，不是 ST 模板默认的 8MHz */
#endif /* HSE_VALUE */
```

同时把 `SystemClock_Config()` 函数头上那段早就过时的说明注释（`HSE Frequency(Hz) = 8000000`、`PLL_M = 8`、`PLL_Q = 7`，这几个值和实际代码里的 `PLLM=25`/`PLLQ=8` 本来就不一致，是模板遗留下来一直没同步的旧注释）一并改成和实际代码一致的数值，避免继续误导后面看代码的人。

### 验证方法

这类"时钟假设和真实晶振不匹配"的问题，没法只靠看代码/看编译结果判断对不对，必须上真实硬件观察：

- 串口能收到正常字符（不再是乱码）；
- 如果之前怀疑过闪灯节奏，同时确认 LED 闪烁间隔是不是也变成了肉眼可辨的、均匀的约 1 次/秒（如果之前偏快，修完应该会明显变慢、变准）。

由于这个偏差是一个固定的乘法系数（本例是 3.125 倍），任何一个使用 `HAL_Delay`/`HAL_GetTick` 做定时、或任何用 HAL 波特率计算公式配置的外设（UART、SPI 等有波特率概念的），在修复前都会一起跟着错、修复后应该一起恢复正常——如果只有串口正常但延时还是不对（或者反过来），说明还有别的独立问题，不能都归因到 `HSE_VALUE` 上。

### 举一反三：以后拿到一块新板子/新模板，先确认这两个数字对不对

以后换板子、或者直接照抄别的项目模板时，`HSE_VALUE`（`stm32f4xx_hal_conf.h`）和 `SystemClock_Config()` 里的 `PLLM`（及其他 PLL 分频系数）必须按板子上**实际**焊的晶振频率成对修改，两者要能对上"PLL 输入 = HSE / PLLM ≈ 1MHz"这条经验规律。只改了 PLL 参数、忘了改 `HSE_VALUE`（或反过来），代码能正常编译、时钟也能正常起振运行，不会有任何报错，但所有依赖 HAL 时钟计算的功能（延时、串口、任何算波特率/周期的外设）都会跟着系统性地跑偏——这是一类"编译和启动都正常，但所有时间/速率相关的东西都不对"的问题，排查时应该优先怀疑这里。

### 问题三的后续（2026-09-11）：想要精确 48MHz，主频从 180MHz 降到了 168MHz

`HSE_VALUE` 修好之后，`PLLQ=8` 给 USB/SDIO/RNG 用的 `CK48M` 时钟实际是 `360MHz / 8 = 45MHz`，不是标准的 48MHz（当时只是为了让 UART 波特率算对，没特别管这个）。回头想优化成精确 48MHz 时，一开始想的方案是：主频继续保持 180MHz，另外配一个独立的 PLLSAI 专门出精确的 48MHz——这在很多 STM32 型号（比如 STM32F469/F479）上是标准做法。

但查完 `drivers/inc/stm32f4xx_hal_rcc_ex.h` 才发现，**这条路在 STM32F429/439 上根本不存在**：`RCC_CLK48CLKSOURCE_PLLSAIP` 这个宏、以及 `RCC_PeriphCLKInitTypeDef` 里的 `Clk48ClockSelection` 字段，头文件里明确写着只在 `#if defined(STM32F469xx) || defined(STM32F479xx)` 才有效；`STM32F429xx`/`STM32F439xx` 分支下的 `RCC_PLLSAIInitTypeDef` 甚至连 `PLLSAIP` 这个字段都没有（只有 N/Q/R，分别给音频 SAI 和 LCD-TFT 用）。也就是说 STM32F429 这颗芯片物理上就没有"PLLSAI 接到 48MHz 外设时钟"这条电路，`CK48M` 只能来自主 PLL 的 `Q` 输出，没有第二个源可选——这是芯片本身的限制，不是 HAL 没封装全。

数学上也证实了"180MHz + 精确48MHz"这个组合本来就凑不出来：主 PLL 的 VCO = `HSE/PLLM*PLLN`，要 `SYSCLK=VCO/PLLP=180MHz` 且 `PLLP∈{2,4,6,8}`，只有 `PLLP=2, VCO=360MHz` 这个组合落在 VCO 允许范围（100~432MHz）内；而要 `CK48M=VCO/PLLQ=48MHz` 且 `PLLQ` 取整数，`VCO` 必须是 48 的整数倍——360 不是 48 的整数倍（`360/48=7.5`），所以单靠这一个 PLL，180MHz 主频和精确 48MHz 外设时钟不可能同时成立。

最终选择：把 VCO 换成 336MHz（`PLLN` 从 360 改成 336），主频跟着降到 `336/2=168MHz`；`CK48M = 336/PLLQ`，`PLLQ` 改成 7，正好是精确的 `48MHz`。168MHz 用的 Flash 等待周期（`FLASH_LATENCY_5`）和之前一样不用改，APB1/APB2 分频后的 42MHz/84MHz 也都在各自总线时钟上限（45MHz/90MHz）以内，比之前 180MHz 时刚好卡在上限还更留了余量。目前项目还没真正用到 USB/SDIO/RNG，这次纯粹是为了"配置本身要精确/标准"、给以后要用这些外设时铺路，不是修一个正在发生的 bug。

### 举一反三

看到"某个时钟源理论上有多种路径可选（比如这里的 PLLSAI）"的资料/经验时，别急着当成通用方案直接抄——一定要先翻这颗芯片自己的 HAL 头文件里对应的 `#if defined(STM32Fxxx)` 分支，确认这个字段/宏在你用的具体型号上真的存在。同一系列不同型号（这里是 F429/439 vs F469/479）的 RCC 外围电路并不总是对齐的。

---

## 小结（问题一、二）：这个项目里"静态库 + 弱符号别名"是一个反复出现的坑

问题一和问题二根因完全一样，只是出现在不同的模块（`core` 和 `bsp`）、覆盖的是不同的符号（`SysTick_Handler` 和 `rt_vsnprintf`）。触发条件都是：

1. 某个符号在两个不同的 `.c`/`.S` 文件里各有一份定义，一份弱、一份强；
2. 这两个文件被编译进**同一个 CMake STATIC 库**；
3. 弱定义所在的文件，因为**自身内部**就会调用/引用这个符号（向量表引用 `SysTick_Handler`、`rt_kprintf` 内部调用 `rt_vsnprintf`），所以这个文件一旦因为别的原因被链接器拉入，弱定义就会"就地"满足引用，链接器不会再去archive 里找强定义。

`app/`、`middlewares/threadx/` 一旦有了真实代码，如果也出现"同一个符号多处定义（弱兜底 + 强覆盖）"的模式，大概率会重复踩到这个坑。当时的应对方式是把涉及到的静态库整体纳入根 `CMakeLists.txt` 的 `-Wl,--whole-archive` / `-Wl,--no-whole-archive` 范围——这只是"哪个模块中了就补哪个"的治标办法。

**后续处理（2026-09-10）**：既然这几个模块库本来就只是用来组织源码、从没打算脱离这个工程单独分发复用，索性把 `app`/`bsp`/`threadx`/`drivers`/`core` 全部从 `add_library(... STATIC)` 改成了 `add_library(... OBJECT)`。OBJECT 库不会打包成 `.a` 归档，链接的时候每一个目标文件都会无条件进最终链接，没有"按需抽取"这一步，也就没有弱符号被截胡的空间——从根上把这一整类坑消除了，根 `CMakeLists.txt` 里的 `--whole-archive`/`--no-whole-archive` 也随之整段删除。改完用 `nm` 复查过 `SysTick_Handler`、`rt_vsnprintf`、`HAL_UART_MspInit` 三个符号，全部是独立地址的强符号，行为和之前一致。唯一的代价是这几个模块以后没法脱离本工程单独打包给别的项目用，但目前没有这个需求。
