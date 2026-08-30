# Two flavours of the same ABI, because the two targets need different things:
#
#   tupista_capi         SHARED — a .dll / .dylib / .so for desktop hosts.
#                        Avalonia loads this through P/Invoke.
#   tupista_capi_static  STATIC — for iOS, which forbids loading arbitrary
#                        native libraries at runtime; the code has to be linked
#                        into the app binary and called as "__Internal".
#
# Same sources, same header, so they can never drift apart.

add_library(tupista_capi SHARED src/tupista.cpp)
add_library(tupista_capi_static STATIC src/tupista.cpp)

foreach(target tupista_capi tupista_capi_static)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    # PRIVATE: the engine is an implementation detail. Nothing that links the
    # ABI should be able to reach the C++ API behind it.
    target_link_libraries(${target} PRIVATE sudoku_core)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endforeach()

# On Windows, symbols must be explicitly exported from a DLL; elsewhere the
# macro expands to nothing. TUPISTA_SHARED is PUBLIC so a C consumer compiling
# against the DLL picks up __declspec(dllimport); TUPISTA_EXPORTS is PRIVATE so
# only our own build flips it to dllexport.
target_compile_definitions(tupista_capi PUBLIC TUPISTA_SHARED PRIVATE TUPISTA_EXPORTS)

# Static libraries linked into a shared one must be position independent, or
# the link fails on Linux and iOS. Harmless on Windows.
set_property(TARGET sudoku_core PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(tupista_capi PROPERTIES OUTPUT_NAME "tupista")
set_target_properties(tupista_capi_static PROPERTIES OUTPUT_NAME "tupista_static")

# .NET resolves DllImport("tupista") to "tupista.dll" on Windows, but adds the
# "lib" prefix itself on Linux and macOS. MinGW would produce libtupista.dll,
# which P/Invoke would never find — so drop the prefix on Windows only and
# leave the conventional one everywhere else.
if(WIN32)
    set_target_properties(tupista_capi PROPERTIES PREFIX "")
endif()

# Put the shared library somewhere the .NET projects can find without knowing
# which build directory produced it — there are several (Rider's, the command
# line's, CI's) and they come and go. artifacts/native is gitignored; whichever
# build ran last wins, which is what you want while developing.
set_target_properties(tupista_capi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/artifacts/native
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/artifacts/native
)
