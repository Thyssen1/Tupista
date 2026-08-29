# Declare oldest CMake version allowed
cmake_minimum_required(VERSION 3.24)

# Tell CMake we only need a C++ compiler
project(Tupista VERSION 1.0.0 LANGUAGES CXX)

# These three lines pin the language level for every target in the project:
#   CMAKE_CXX_STANDARD 20         -> compile as C++20
#   CMAKE_CXX_STANDARD_REQUIRED   -> fail if the compiler can't do C++20,
#                                    instead of silently falling back
#   CMAKE_CXX_EXTENSIONS OFF      -> use -std=c++20, not -std=gnu++20; keeps us
#                                    off GCC-only extensions, which matters for
#                                    portability (MSVC for WPF, Clang for iOS)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core is a static library for tests and for iOS later
add_subdirectory(core)

# enable_testing() turns on CTest support for this build tree, so "ctest" can
# discover the tests registered with add_test(). It must appear BEFORE the
# subdirectory that calls add_test().
enable_testing()
add_subdirectory(tests)
