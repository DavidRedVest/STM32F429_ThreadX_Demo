# STM32F429_ThreadX_Demo

面向 STM32F429（Cortex-M4）的裸机固件模板，目标是整合 STM32F4xx HAL 与 Eclipse ThreadX（RTOS），
使用 CMake + Ninja 和 `arm-none-eabi` GCC 工具链构建。

目前 `core/`、`drivers/`、`bsp/`、`app/` 都已有可用源码，能跑起来一个闪灯 + 串口打印的 demo；
只有 `middlewares/threadx/` 还是空的，等真正接入 ThreadX 内核。`core/src/main.c` 只是个入口：
`HAL_Init()` → `SystemClock_Config()` → `app_init()`，然后 `while(1) app_task();`——真正的板级/
业务逻辑（LED 闪烁、USART1 打印）都在 `app/src/app.c` 和 `bsp/` 里。

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
├── bsp/                        # 已实现：板级支持层——GPIO LED、USART1（含 rt_kprintf 打印）
│   ├── CMakeLists.txt
│   ├── inc/
│   └── src/
├── middlewares/
│   └── threadx/                # 尚未移植：预留给 ThreadX 内核 + Cortex-M4 GNU port
│       └── CMakeLists.txt
├── app/                        # 已实现：应用层，app_init()（LED/串口初始化）+ app_task()（主循环体）
│   ├── CMakeLists.txt
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

## 串口打印

`bsp/` 里移植了一份 RT-Thread 的 `rt_kprintf`（`bsp/inc/rtthread.h` + `bsp/src/kservice.c`/
`rt_vsnprintf.c`），通过 USART1（PA9=TX，PA10=RX）输出，`app_init()` 里已经调了几行 demo 打印。
用串口终端连接开发板，参数 **115200, 8N1**，上电即可看到输出。移植过程中踩过的坑（`uart.h` 头文件名、
`GPIO_SPEED_FAST` 宏不存在等）记在 `docx/TROUBLESHOOTING.md` 里。
