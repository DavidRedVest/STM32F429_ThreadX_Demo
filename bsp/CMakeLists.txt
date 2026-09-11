add_library(bsp OBJECT)

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

# usmart.c 里通过 u32(*)() 这种"无原型"函数指针来适配任意参数个数的函数调用,
# 这在 C17 及更早标准里是"参数个数不确定",可以传任意实参;但 GCC 15 默认的
# -std=gnu23 把空括号形参列表改成了和 (void) 一样的"零个参数",导致这个经典
# 移植技巧变成硬编译错误("too many arguments")。为了不去改动这一大段已经
# 按参数个数分好 case 的移植代码,直接把这一个文件退回 -std=gnu17 编译。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/usmart.c")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/usmart.c"
        PROPERTIES COMPILE_OPTIONS "-std=gnu17")
endif()
