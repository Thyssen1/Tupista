add_library(sudoku_core STATIC
    src/board.cpp
    src/validate.cpp
    src/rules.cpp
    src/solver.cpp
)

target_include_directories(sudoku_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(MSVC)
    target_compile_options(sudoku_core PRIVATE /W4)
else()
    target_compile_options(sudoku_core PRIVATE -Wall -Wextra)
endif()
