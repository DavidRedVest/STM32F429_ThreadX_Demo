add_library(threadx STATIC)

# 收集 ThreadX 内核通用源码与 Cortex-M4 GNU 汇编
file(GLOB TX_COMMON_SOURCES "common/src/*.c")
file(GLOB TX_PORT_SOURCES "ports/cortex_m4/gnu/src/*.c" "ports/cortex_m4/gnu/src/*.S")

set(TX_SOURCES ${TX_COMMON_SOURCES} ${TX_PORT_SOURCES})
if(NOT TX_SOURCES)
    # ThreadX 尚未移植，占位空文件让空库先能配置/链接通过
    set(TX_STUB "${CMAKE_CURRENT_BINARY_DIR}/threadx_stub.c")
    file(WRITE "${TX_STUB}" "")
    list(APPEND TX_SOURCES "${TX_STUB}")
endif()

target_sources(threadx PRIVATE
    ${TX_SOURCES}
)

target_include_directories(threadx PUBLIC
    common/inc
    ports/cortex_m4/gnu/inc
    ${CMAKE_SOURCE_DIR}/core/inc # 包含 tx_user.h
)

target_compile_definitions(threadx PUBLIC 
    TX_INCLUDE_USER_DEFINE_FILE
)

target_link_libraries(threadx PUBLIC stm32_mcu_flags)