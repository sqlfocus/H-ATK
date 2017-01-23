#指定源文件
aux_source_directory(. MAIN_SRCS)

#指定生成目标
add_executable(H-ATK ${MAIN_SRCS})
target_link_libraries(H-ATK libcjson.a m)     #m: cjson使用了floor/pow等数学库




