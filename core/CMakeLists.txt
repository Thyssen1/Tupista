add_library(sudoku_core STATIC
    src/hint_engine.cpp
)

target_include_directories(sudoku_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)