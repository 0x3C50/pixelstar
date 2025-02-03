cmake_minimum_required(VERSION 3.30)
project(pixelstar)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-Ofast")
find_package(Seastar REQUIRED)

add_executable(pixelstar
        src/entry.cpp
        src/pixelstar.cpp
)

target_link_libraries(pixelstar PUBLIC Seastar::seastar)
target_include_directories(pixelstar PUBLIC "include")
