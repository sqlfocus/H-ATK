#指定源文件
include_directories(${CMAKE_SOURCE_DIR}/src)
aux_source_directory(. MAIN_SRCS)

#生成HTTP攻击目标
aux_source_directory(./http HTTP_WORKER_SRCS)
add_executable(H-ATK ${MAIN_SRCS} ${HTTP_WORKER_SRCS})
target_link_libraries(H-ATK libcjson.a m)     #m: cjson使用了floor/pow等数学库




