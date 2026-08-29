// doctest is header-only: the entire framework lives in doctest.h, but its
// implementation (test registry, runner, main()) must be compiled into
// EXACTLY ONE translation unit. This #define, before the include, makes this
// file that one unit — including generating main() for us. Every other test
// file just includes <doctest/doctest.h> plain; defining this twice would
// give duplicate-symbol linker errors.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>