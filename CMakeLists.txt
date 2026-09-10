cmake_minimum_required(VERSION 3.22)

# 必须在 project() 之前引入工具链定义
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake")

project(stm32f429_firmware C CXX ASM)

# 生成 compile_commands.json，供 VSCode clangd 插件做跳转/补全用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全局架构与编译选项
set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard 
)

# 接口库：集中管理全局通用编译与链接参数
add_library(stm32_mcu_flags INTERFACE)
target_compile_options(stm32_mcu_flags INTERFACE
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -fdata-sections
    -ffunction-sections
)

# 引入链接脚本
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/STM32F429IGT6_FLASH.ld")
target_link_options(stm32_mcu_flags INTERFACE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map,--cref 
    -Wl,--gc-sections 
    -specs=nano.specs 
    -specs=nosys.specs 
)

# 递归包含各模块子目录
add_subdirectory(drivers)
add_subdirectory(bsp)
add_subdirectory(middlewares/threadx)
add_subdirectory(app)
add_subdirectory(core)

# 生成可执行目标（ELF）
# add_executable 自身不持有源码（main 等真正代码来自 core 等 OBJECT 库），
# 但 CMake 要求可执行目标至少有一个源文件，故用空文件占位。
set(EXE_STUB "${CMAKE_CURRENT_BINARY_DIR}/exe_stub.c")
file(WRITE "${EXE_STUB}" "")
add_executable(${CMAKE_PROJECT_NAME} "${EXE_STUB}")
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)

# app/bsp/threadx/drivers/core are all OBJECT libraries (not STATIC), so every
# one of their object files always lands in the final link unconditionally —
# no archive extraction, no risk of a weak definition (e.g. a vector-table
# alias) silently shadowing a strong override that lives in a different
# source file of the same module (see docx/TROUBLESHOOTING.md, problems 1-2).
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    stm32_mcu_flags
    app
    threadx
    drivers
    bsp
    core
)

# 编译后生成 .bin .hex 与内存占用统计 
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin 
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Building Hex/Bin and printing firmware size"
)