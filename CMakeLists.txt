#最低版本要求
cmake_minimum_required(VERSION 2.8)

#项目信息
project(H-ATK C)

#添加编译选项
ADD_DEFINITIONS(-Wall)
ADD_DEFINITIONS(-Werror)
ADD_DEFINITIONS(-Wunused)
ADD_DEFINITIONS(-std=gnu99)

#指定Debug编译模式("$ cmake -DCMAKE_BUILD_TYPE=Debug ..")
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug)         #开发期间默认为Debug模式
    #set(CMAKE_BUILD_TYPE Release)
endif() 
set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -g -ggdb")
set(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3")

#指定编译结果生成路径
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

#创建定制目标, 编译依赖对象
set(DEPS_INCLUDE "${CMAKE_BINARY_DIR}/deps/include")
set(DEPS_LIB "${CMAKE_BINARY_DIR}/deps/lib")
set(DEPS_BIN "${CMAKE_BINARY_DIR}/bin")
file(MAKE_DIRECTORY ${DEPS_LIB})
file(MAKE_DIRECTORY ${DEPS_INCLUDE})
file(MAKE_DIRECTORY ${DEPS_BIN})
link_directories(${DEPS_LIB})
include_directories(${DEPS_INCLUDE})

set(CJSON_SRC "${CMAKE_SOURCE_DIR}/deps/cJSON")
set(CJSON_LIB_A "libcjson.a")
set(CJSON_LIB_A_H "cJSON.h")
if(NOT EXISTS ${DEPS_LIB}/${CJSON_LIB_A})
    message(STATUS "building ${CJSON_SRC}")

    execute_process(
        COMMAND make clean
        WORKING_DIRECTORY ${CJSON_SRC})
    execute_process(
        COMMAND make static
        WORKING_DIRECTORY ${CJSON_SRC})
    execute_process(
        COMMAND cp ${CJSON_LIB_A}  ${DEPS_LIB}
        WORKING_DIRECTORY ${CJSON_SRC})
    execute_process(
        COMMAND cp ${CJSON_LIB_A_H}  ${DEPS_INCLUDE}
        WORKING_DIRECTORY ${CJSON_SRC})
    
    message(STATUS "done")
endif()

set(LIBEV_SRC "${CMAKE_SOURCE_DIR}/deps/libev-4.24")
set(LIBEV_LIB_A "libev.a")
set(LIBEV_LIB_A_H "ev.h")
if(NOT EXISTS ${DEPS_LIB}/${LIBEV_LIB_A})
    message(STATUS "building ${LIBEV_SRC}")

    execute_process(
        COMMAND ./configure
        WORKING_DIRECTORY ${LIBEV_SRC})
    execute_process(
        COMMAND make
        WORKING_DIRECTORY ${LIBEV_SRC})
    execute_process(
        COMMAND cp .libs/${LIBEV_LIB_A}  ${DEPS_LIB}
        WORKING_DIRECTORY ${LIBEV_SRC})
    execute_process(
        COMMAND cp ${LIBEV_LIB_A_H}  ${DEPS_INCLUDE}
        WORKING_DIRECTORY ${LIBEV_SRC})
    
    message(STATUS "done")
endif()


#指定编译子目录
add_subdirectory(src)


