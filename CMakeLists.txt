cmake_minimum_required(VERSION 3.10)
project(new-codec CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(codec STATIC
    src/transform.cpp
    src/entropy.cpp
    src/encoder.cpp
    src/decoder.cpp
)
target_include_directories(codec PUBLIC src)

add_executable(nc src/main.cpp)
target_link_libraries(nc PRIVATE codec)

enable_testing()
add_executable(test_roundtrip tests/test_roundtrip.cpp)
target_link_libraries(test_roundtrip PRIVATE codec)
add_test(NAME roundtrip COMMAND test_roundtrip)