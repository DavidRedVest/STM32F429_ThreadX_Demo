add_library(drivers STATIC)

# 收集 HAL 驱动代码
file(GLOB HAL_SOURCES "src/*.c")

# stm32f4xx_hal_eth.c 依赖 Legacy/stm32_hal_legacy.h 里的 PHY_READ_TO/PHY_WRITE_TO 宏，
# 该头文件未随驱动一起引入（暂不需要以太网）；如需 ETH 功能，把该文件加回来并定义这两个宏
list(REMOVE_ITEM HAL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4xx_hal_eth.c")

target_sources(drivers PRIVATE
    ${HAL_SOURCES}
)

# 导出 drivers 的头文件路径供上层使用
# core/inc 下的 stm32f4xx_hal_conf.h 是工程级 HAL 配置头（CubeMX 惯例位置），
# HAL 驱动源码里 #include "stm32f4xx_hal.h" 需要能找到它，故一并加入 include 路径
# （仅头文件搜索路径，不建立 drivers -> core 的目标链接依赖）
target_include_directories(drivers PUBLIC
    inc
    cminc
    ${CMAKE_SOURCE_DIR}/core/inc
)

target_compile_definitions(drivers PUBLIC 
    STM32F429xx 
    USE_HAL_DRIVER
)

target_link_libraries(drivers PUBLIC stm32_mcu_flags)
