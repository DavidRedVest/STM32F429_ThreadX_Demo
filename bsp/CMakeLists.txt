add_library(bsp STATIC)

file(GLOB BSP_SOURCE "src/*.c")
target_sources(bsp PRIVATE ${BSP_SOURCE})

target_include_directories(bsp PUBLIC inc)

# BSP 依赖底层 Drivers 库
target_link_libraries(bsp PUBLIC drivers stm32_mcu_flags)
