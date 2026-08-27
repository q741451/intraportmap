cmake_minimum_required(VERSION 3.10)

project(intraportmap VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
# 源文件是无 BOM 的 UTF-8。MSVC 默认按系统代码页（中文环境下即 GBK）
# 解读，一行中文数为奇数时末尾字节会去与行尾配对，注释会吞掉换行
add_compile_options("/utf-8")
set (CMAKE_CONFIGURATION_TYPES "Release;Debug")
set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_RELEASE
   )
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()
endif(MSVC)

find_package(Libevent REQUIRED)
find_path(LIBEVENT_INCLUDE_DIR event2/event.h REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIR})

add_subdirectory(src)







