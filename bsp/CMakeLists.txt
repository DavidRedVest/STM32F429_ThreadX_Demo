add_library(bsp STATIC)

file(GLOB BSP_SOURCE "src/*.c")
if(NOT BSP_SOURCE)
    # bsp 尚未移植，占位空文件让空库先能配置/链接通过
    set(BSP_STUB "${CMAKE_CURRENT_BINARY_DIR}/bsp_stub.c")
    file(WRITE "${BSP_STUB}" "")
    list(APPEND BSP_SOURCE "${BSP_STUB}")
endif()
target_sources(bsp PRIVATE ${BSP_SOURCE})

target_include_directories(bsp PUBLIC inc)

# BSP 依赖底层 Drivers 库
target_link_libraries(bsp PUBLIC drivers stm32_mcu_flags)
