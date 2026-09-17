# STM32F429_ThreadX_Demo

面向 STM32F429（Cortex-M4）的裸机固件模板，整合了 STM32F4xx HAL 与 Eclipse ThreadX（RTOS），
使用 CMake + Ninja 和 `arm-none-eabi` GCC 工具链构建。

`core/`、`drivers/`、`bsp/`、`app/`、`middlewares/threadx/` 现在都有可用源码，ThreadX 内核 +
Cortex-M4 GNU 端口已经真正跑起来：`core/src/main.c` 只是个入口——`HAL_Init()` →
`SystemClock_Config()` → `app_init()`，`app_init()` 先做完 LED/串口/按键/usmart 控制台这些外设
初始化，最后调用 `tx_kernel_enter()` 进入 ThreadX 内核，正常情况下不会返回（`main.c` 里
`while(1) app_task();` 只是 ThreadX 启动失败时的兜底路径）。真正的板级/业务逻辑（LED 闪烁、按键
检测、串口打印）都在 `app/src/app.c` 和 `bsp/` 里，以 ThreadX 线程的形式运行——详见下面的
"ThreadX 任务" 一节。

工程目录结构示意图如下：

```text
STM32F429_ThreadX_Demo/
├── CMakeLists.txt              # 顶层根构建入口（负责全局选项、定义目标与子模块装配）
├── jlink.cfg                   # J-Link 烧录命令文件
├── cmake/                      # CMake 支撑脚本目录
│   ├── arm-none-eabi.cmake     # 交叉编译工具链定义（Toolchain File）
│   └── STM32F429IGT6_FLASH.ld  # 链接脚本
│
├── drivers/                    # 已实现：vendored STM32F4xx HAL/LL 驱动 + CMSIS 头文件
│   ├── CMakeLists.txt
│   ├── src/
│   ├── inc/
│   └── cminc/
├── bsp/                        # 已实现：板级支持层——GPIO LED、USART1（含 rt_kprintf 打印）、
│   ├── CMakeLists.txt          #   按键（TIM7 扫描）、usmart 串口调试控制台（TIM4 扫描）
│   ├── inc/
│   └── src/
├── middlewares/
│   └── threadx/                # 已实现：ThreadX 内核（common/）+ Cortex-M4 GNU 端口（ports/）
│       ├── CMakeLists.txt
│       ├── common/
│       └── ports/
├── app/                        # 已实现：应用层，app_init()（外设初始化 + 进入 ThreadX 内核）+
│   ├── CMakeLists.txt          #   tx_application_define()/AppTaskCreate()（建 ThreadX 线程）
│   ├── inc/
│   └── src/
└── core/                       # 已实现：CMSIS 启动/系统文件 + main.c/中断服务函数/HAL MSP/HAL 时基（TIM6）
    ├── CMakeLists.txt
    ├── inc/
    └── src/
```

## 构建

需要 `arm-none-eabi-gcc` 工具链（本项目在 15.3.1 上验证过）以及 `cmake`/`ninja`。若工具链不在
`PATH` 上，把它加进 shell profile（例如 `~/.zshrc`）：

```bash
export PATH="/path/to/arm-gnu-toolchain-*/bin:$PATH"
```

在终端执行以下命令进行编译生成：

```bash
# 生成 Ninja 构建规则并指定输出目录
cmake -B build -G Ninja

# 执行多核并行构建
cmake --build build
```

构建完成后会在 `build/` 下生成 `stm32f429_firmware.elf/.hex/.bin`，并打印固件体积（Flash/RAM 占用）。

目前没有配置测试套件或 linter。

## 烧录

根目录的 `jlink.cfg` 是一个 J-Link 命令文件，通过 SWD 把 `build/stm32f429_firmware.bin` 写入
`0x08000000`（目标芯片 `stm32f429ig`），校验后复位运行：

```bash
JLinkExe -CommandFile jlink.cfg
```

先执行 `cmake --build build` 生成 `.bin`，再运行上面的命令烧录。

## ThreadX 任务

`app/src/app.c` 的 `tx_application_define()`/`AppTaskCreate()` 建了 5 个线程（数字越小优先级越高）：

| 线程          | 优先级 | 作用                                             |
|---------------|:------:|--------------------------------------------------|
| AppTaskStart  |   2    | 启动任务：建其它线程/互斥量，建完后常驻不退出     |
| AppTaskLed    |   3    | 每 500ms 切换一次 LED1/LED2                       |
| AppTaskKey    |   3    | 每 5ms 轮询一次按键 FIFO，打印按键事件            |
| AppTaskStat   |  30    | 统计 CPU 占用率（`OSCPUUsage`）                   |
| AppTaskIDLE   |  31    | 空闲任务，仅用于统计空闲时间                      |

按下 **WKUP** 按键会调用 `DispTaskInfo()`，通过串口打印当前所有线程的优先级、栈大小、当前/历史最大
栈使用量，方便调试任务栈是否够用。这个功能依赖 `middlewares/threadx/ports/inc/tx_user.h` 里的
`TX_ENABLE_STACK_CHECKING`（否则"历史最大栈使用量"这一列不会更新）。

## 串口打印

`bsp/` 里移植了一份 RT-Thread 的 `rt_kprintf`（`bsp/inc/rtthread.h` + `bsp/src/kservice.c`/
`rt_vsnprintf.c`），通过 USART1（PA9=TX，PA10=RX）输出，`app_init()` 里已经调了几行 demo 打印。
`rt_kprintf` 本身不是线程安全的（内部用了一个共享的静态缓冲区），多个 ThreadX 线程要打印时要通过
`app/src/app.c` 里的 `App_Printf()`（`tx_mutex` 互斥保护）而不是直接调 `rt_kprintf`。

同一路 USART1 上还跑着一个 ALIENTEK 风格的 `usmart` 调试控制台（`bsp/src/usmart.c`/
`usmart_str.c`），靠 TIM4 定时中断每 100ms 扫描一次；已注册的函数（见 `app/src/usmart_config.c`）
可以直接在串口终端里按名字调用，比如输入 `list` 看已注册函数列表，`led_set(1)` 点亮 LED1。

用串口终端连接开发板，参数 **115200, 8N1**，上电即可看到输出。移植过程中踩过的坑（`uart.h` 头文件名、
`GPIO_SPEED_FAST` 宏不存在、ThreadX 移植时的符号名不匹配/中断处理函数冲突等）都记在
`docx/TROUBLESHOOTING.md` 里。
