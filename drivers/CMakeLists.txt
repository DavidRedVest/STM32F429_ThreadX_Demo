add_library(drivers STATIC)

# 收集 HAL 驱动与CMSIS启动/时钟代码
file(GLOB HAL_SOURCES "src/*.c")
set(STARTUP_SOURCE "../core/src/startup_stm32f429xx.S")
set(SYSTEM_SOURCE  "../core/src/system_stm32f4xx.c")

target_sources(drivers PRIVATE 
    ${HAL_SOURCES}
    ${STARTUP_SOURCE}
    ${SYSTEM_SOURCE}
)

# 导出 drivers 的头文件路径供上层使用
target_include_directories(drivers PUBLIC 
    inc 
    cminc
)

target_compile_definitions(drivers PUBLIC 
    STM32F429xx 
    USE_HAL_DRIVER
)

target_link_libraries(drivers PUBLIC stm32_mcu_flags)
