cmake_minimum_required(VERSION 3.27.0)

project(hex2bin)

set(PROJECT_VERSION 1.0.1)
set(CMAKE_BUILD_TYPE Debug)

include_directories(src)

add_executable(hex2bin src/hex2bin.c src/binary.c src/checksum.c src/common.c src/libcrc.c)
add_executable(mot2bin src/mot2bin.c src/binary.c src/checksum.c src/common.c src/libcrc.c)