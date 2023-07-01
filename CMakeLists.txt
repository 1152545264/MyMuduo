cmake_minimum_required(VERSION 2.5)
project(mymuduo)


# cmake  ==> makefile ==> make
# mymuduo最终编译成so动态库 设置动态库的路径 放在根目录的lib文件夹下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#设置调试信息 以及开启C++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

# cmake 设置debug版本 参考自：https://blog.csdn.net/lcalqf/article/details/74835118
set(CMAKE_BUILD_TYPE "Debug")

# 定义参与编译的源代码文件
aux_source_directory(. SRC_LIST)

# 编译生成动态库
add_library(mymuduo SHARED ${SRC_LIST})