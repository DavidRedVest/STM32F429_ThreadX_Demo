# STM32F429_ThreadX_Demo

面向 STM32F429（Cortex-M4）的裸机固件模板，目标是整合 STM32F4xx HAL 与 Eclipse ThreadX（RTOS），
使用 CMake + Ninja 和 `arm-none-eabi` GCC 工具链构建。

项目目前处于早期搭建阶段：`app/`、`middlewares/threadx/` 下只有 `CMakeLists.txt`，还没有真正的
源码；`core/`、`drivers/`（vendored 的 STM32F4xx HAL/LL + CMSIS）已有可用源码；`bsp/` 刚起步，
目前只有一个最小的 GPIO LED 驱动（`bsp_led.c`/`.h`），被 `core/src/main.c` 拿来做闪灯 demo。

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
├── bsp/                        # 部分实现：板级支持层，目前只有 GPIO LED 驱动
│   ├── CMakeLists.txt
│   ├── inc/
│   └── src/
├── middlewares/
│   └── threadx/                # 尚未移植：预留给 ThreadX 内核 + Cortex-M4 GNU port
│       └── CMakeLists.txt
├── app/                        # 尚未开发：应用/业务逻辑层
│   ├── CMakeLists.txt
│   ├── inc/
│   └── src/
└── core/                       # 已实现：CMSIS 启动/系统文件 + main.c/中断服务函数/HAL MSP
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
