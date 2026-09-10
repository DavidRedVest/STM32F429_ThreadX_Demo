

工程目录结构示意图如下：

```text
stm32f429_template/
├── CMakeLists.txt              # 1. 顶层根构建入口（负责全局选项、定义目标与子模块装配）
├── cmake/                      # 2. CMake 支撑脚本目录
│   ├── arm-none-eabi.cmake     # 交叉编译工具链定义（Toolchain File）
│   └── STM32F429IGTx_FLASH.ld  # 链接脚本（推荐集中放置在 cmake/ 目录下）
│
├── Drivers/
│   ├── CMakeLists.txt          # [可选] Drivers 子模块构建脚本（生成 DriversTarget 静态库/接口）
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
├── BSP/
│   ├── CMakeLists.txt          # [可选] BSP 子模块构建脚本
│   ├── Inc/
│   └── Src/
├── Middlewares/
│   └── ThreadX/
│       └── CMakeLists.txt      # ThreadX 内核构建脚本
├── App/
│   ├── CMakeLists.txt          # [可选] App 业务逻辑构建脚本
│   ├── Inc/
│   └── Src/
└── Core/
    ├── Inc/
    └── Src/

```

**构建执行流程**
在终端执行以下命令进行编译生成：

```bash
# 生成 Ninja 构建规则并指定输出目录
cmake -B build -G Ninja

# 执行多核并行构建
cmake --build build
```