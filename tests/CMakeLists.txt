include(FetchContent)
FetchContent_Declare(
        doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
)
FetchContent_MakeAvailable(doctest)

add_executable(sudoku_tests
        src/main.cpp
        src/BoardTests.cpp
        src/ValidateTests.cpp
        src/SolverTests.cpp
)

# Linking sudoku_core also inherits its PUBLIC include directory (that is how
# #include "sudoku/board.hpp" resolves here). doctest::doctest is the target
# the doctest project exports; linking it supplies the header path for
# #include <doctest/doctest.h>. PRIVATE = these are implementation details of
# the test exe; nothing links against a test exe anyway.
target_link_libraries(sudoku_tests PRIVATE
        sudoku_core
        doctest::doctest
)

# Registers the executable with CTest under the name sudoku_tests, so
# "ctest --test-dir <builddir>" knows to run it and treat a non-zero exit
# code as failure. Without this line the exe still exists but ctest reports
# "no tests found".
add_test(NAME sudoku_tests COMMAND sudoku_tests)