cmake_minimum_required(VERSION 3.24)
project(Tupista VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core is a static library for tests and for iOS later 
add_subdirectory(core)


enable_testing()
add_subdirectory(tests)
