add_library(threasx STATIC)

# 收集 ThreadX 内核通用源码与 Cortex-M4 GNU 汇编 
file(GLOB TX_COMMON_SOURCES "common/src/*.c")
file(GLOB TX_PORT_SOURCES "ports/cortex_m4/gnu/src/*.c" "ports/cortex_m4/gnu/src/*.S")

target_sources(threadx PRIVATE 
    ${TX_COMMON_SOURCES}
    $(TX_PORT_SOURCES)
)

target_include_directories(threadx PUBLIC 
    common/inc 
    ports/cortex_m4/gnu/inc 
    ${CMAKE_SOURCE_DIR}/Core/Inc # 包含 tx_user.h
)

target_compile_definitions(threadx PUBLIC 
    TX_INCLUDE_USER_DEFINE_FILE
)

target_link_libraries(threadx PUBLIC stm32_mcu_flags)