add_executable(sudoku_tests
        src/EngineTests.cpp
)

target_link_libraries(sudoku_tests PRIVATE
        sudoku_core
)