cmake_minimum_required(VERSION 3.23)
project(gazo_san)

set(CMAKE_CXX_STANDARD 17)

include_directories(.)

add_executable(gazo-san
        gazo-san.h
        main.cc cmdline.cc)
