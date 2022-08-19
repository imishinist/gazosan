cmake_minimum_required(VERSION 3.23)
project(gazo-san)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)
add_subdirectory("third-party/oneTBB")
# include_directories(third-party/oneTBB/include)

add_executable(gazo-san
        gazo-san.h
        main.cc cmdline.cc strerror.cc image.cc perf.cc)

# TODO: use third-party directory (now use pkg-config)
find_package(PkgConfig)
pkg_check_modules(LIBTBB REQUIRED tbb)
if (LIBTBB_FOUND)
    include_directories(${LIBTBB_INCLUDE_DIRS})
    target_link_libraries(gazo-san ${LIBTBB_LINK_LIBRARIES})
endif()

find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    target_include_directories(gazo-san PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(gazo-san ${OpenCV_LIBS})
endif()