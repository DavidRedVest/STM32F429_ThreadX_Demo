cmake_minimum_required(VERSION 3.22)

# 必须在 project() 之前引入工具链定义
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake")

project(stm32f429_firmware C CXX ASM)

# 全局架构与编译选项
set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard 
)

# 接口库：集中管理全局通用编译与链接参数
add_library(stm32_muc_flags INTERFACE)
target_compile_options(stm32_mcu_flags INTERFACE 
    $(MCU_FLAGS)
    -Wall 
    -Wextra 
    -fdata-sections 
    -ffunction-se
    
# 引入链接脚本 
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/STM32F429IGTx_FLASH.ld")
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
add_subdirectory(middleeares/threadx)
add_subdirectory(app)
add_subdirectory(core)

# 生成可执行目标（ELF）
add_executable(${CMAKE_PROJECT_NAME})

# 链接各个解耦模块 
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    stm32_mcu_flags
    app 
    bsp 
    threadx 
    drivers 
    core 
)

# 编译后生成 .bin .hex 与内存占用统计 
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin 
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMAND "Building Hex/Bin and printing firmware size"
)