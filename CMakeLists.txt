cmake_minimum_required(VERSION 3.23)
project(gazosan)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)

add_executable(gazosan)

target_sources(gazosan PRIVATE
        cmdline.cc
        image.cc
        main.cc
        perf.cc
        strerror.cc
)

find_package(TBB REQUIRED)
target_link_libraries(gazosan PRIVATE TBB::tbb)

find_package(OpenCV REQUIRED)
target_link_libraries(gazosan PRIVATE ${OpenCV_LIBS})
