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
        src/HumanSolverTests.cpp
        src/HintTests.cpp
        src/AuditTests.cpp
        src/CapiTests.cpp
)

# Linking sudoku_core also inherits its PUBLIC include directory (that is how
# #include "sudoku/board.hpp" resolves here). doctest::doctest is the target
# the doctest project exports; linking it supplies the header path for
# #include <doctest/doctest.h>. PRIVATE = these are implementation details of
# the test exe; nothing links against a test exe anyway.
# The static flavour of the C ABI, so the boundary is exercised by the same
# test run. Linking the static one keeps the test executable free of a DLL
# copying step; the shared one is what the Avalonia app will load.
target_link_libraries(sudoku_tests PRIVATE
        sudoku_core
        tupista_capi_static
        doctest::doctest
)

# By default doctest forward-declares pieces of the standard library instead of
# including the real headers, to keep compile times down. Recent MSVC standard
# libraries reject that (you get a wall of errors inside <__msvc_string_view.hpp>
# about an undefined std::basic_ostream). This macro tells doctest to include
# the genuine headers. Costs a little compile time, works on every compiler.
target_compile_definitions(sudoku_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)

# Registers the executable with CTest under the name sudoku_tests, so
# "ctest --test-dir <builddir>" knows to run it and treat a non-zero exit
# code as failure. Without this line the exe still exists but ctest reports
# "no tests found".
add_test(NAME sudoku_tests COMMAND sudoku_tests)