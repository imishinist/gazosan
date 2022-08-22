cmake_minimum_required(VERSION 2.8)
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

# Setup TBB
# ported from https://github.com/rui314/mold
option(GAZOSAN_USE_SYSTEM_TBB "Use system or vendored TBB" OFF)
if(GAZOSAN_USE_SYSTEM_TBB)
  find_package(TBB REQUIRED)
  target_link_libraries(gazosan PRIVATE TBB::tbb)
else()
  function(gazosan_add_tbb)
    set(BUILD_SHARED_LIBS OFF)
    set(TBB_TEST OFF CACHE INTERNAL "")
    set(TBB_STRICT OFF CACHE INTERNAL "")
    add_subdirectory(third-party/tbb EXCLUDE_FROM_ALL)
    target_compile_definitions(tbb PRIVATE __TBB_DYNAMIC_LOAD_ENABLED=0)
    target_link_libraries(gazosan PRIVATE TBB::tbb)
  endfunction()

  gazosan_add_tbb()
endif()

find_package(OpenCV REQUIRED)
target_link_libraries(gazosan PRIVATE ${OpenCV_LIBS})
