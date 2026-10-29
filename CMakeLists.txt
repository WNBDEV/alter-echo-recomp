cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Alter Echo - Static Recompilation
#
# Copy this template to your game project root and customize:
#   1. Change the project name below
#   2. Set XBOXRECOMP_DIR to where you cloned xboxrecomp
#   3. Adjust source file lists as you add game-specific code
#   4. Add any game-specific compile definitions or link libraries
# ============================================================================

project(alter_echo_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common definitions
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── xboxrecomp toolkit ──────────────────────────────────────────
#
# Option A: xboxrecomp as a subdirectory (simplest)
#   Place xboxrecomp/ next to your game project and use a relative path.
#
# Option B: find_package (if you install xboxrecomp system-wide)
#   find_package(xboxrecomp REQUIRED)
#
# Adjust this path to point to your xboxrecomp clone:
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

# Add xboxrecomp as a subdirectory -- this builds all 6 runtime libraries
# and provides the 'xboxrecomp' umbrella target to link against.
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# The versions the log header names when the game is started without run.sh
# (src/launch.c), read when CMake configures -- setup.sh always does.
execute_process(COMMAND git describe --always --dirty
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE AE_BUILD_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git describe --always --dirty
                WORKING_DIRECTORY ${XBOXRECOMP_DIR}
                OUTPUT_VARIABLE AE_TOOLKIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

# ── Recompiled game executable ──────────────────────────────────

# Collect generated recompiled code.
# The recomp pipeline outputs files into src/recomp/gen/.
# With --split 1000, you get gen_0000.c, gen_0001.c, etc.
# CONFIGURE_DEPENDS so re-running the recompiler picks up added/removed files
# without a manual re-configure. A plain GLOB is evaluated once at configure
# time, so a newly generated file (an extra chunk, recomp_stubs_unresolved.c)
# is silently left out of the build and shows up as unresolved externals.
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")
# Largest first. Ninja starts files in this order, and the three biggest take
# up to two minutes each: started last, a parallel build waits on them at the
# end with every other thread idle.
set(_gen_by_size "")
foreach(_f IN LISTS RECOMP_GEN_SOURCES)
    file(SIZE "${_f}" _size)
    list(APPEND _gen_by_size "${_size}|${_f}")
endforeach()
list(SORT _gen_by_size COMPARE NATURAL ORDER DESCENDING)
list(TRANSFORM _gen_by_size REPLACE "^[0-9]+\\|" "" OUTPUT_VARIABLE RECOMP_GEN_SOURCES)

# Core game sources
set(GAME_SOURCES
    src/main.c
    src/recomp_manual.c
    # Runtime-loaded modules (stage 2): the loader hook and the generated
    # layout table (tools/build_modules.py writes module_layout.c).
    src/module_loader.c
    src/module_layout.c
    src/module_thunks.c
    # RECOMP_ARGS: the game's development command line (level warp, -bypass).
    src/game_args.c
    # Keyboard help for the game's own modes (WASD in time dilation).
    src/title_input.c
    # Started without a launcher script (Windows double-click): settings, log.
    src/launch.c
    # Add game-specific sources here, e.g.:
    # src/asset_loader.c
    # src/custom_renderer.c
)

# The recomp pipeline also generates a dispatch header and function declarations.
# These are included by the generated .c files:
#   src/recomp/gen/recomp_funcs.h    - extern declarations for all recompiled functions
#   src/recomp/recomp_dispatch.h     - binary search dispatch table
#   src/recomp/recomp_types.h        - register model, ICALL macros, MEM helpers

add_executable(${PROJECT_NAME} WIN32
    ${GAME_SOURCES}
    ${RECOMP_GEN_SOURCES}
)
if(AE_BUILD_DESCRIBE)
    set_property(SOURCE src/launch.c APPEND PROPERTY COMPILE_DEFINITIONS
                 "AE_BUILD_DESCRIBE=\"${AE_BUILD_DESCRIBE}\"")
endif()
if(AE_TOOLKIT_DESCRIBE)
    set_property(SOURCE src/launch.c APPEND PROPERTY COMPILE_DEFINITIONS
                 "AE_TOOLKIT_DESCRIBE=\"${AE_TOOLKIT_DESCRIBE}\"")
endif()


# Link against the xboxrecomp umbrella target (all 6 runtime libraries)
target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)

# Include paths for generated recomp headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${XBOXRECOMP_DIR}/include
)

if(WIN32)
    # Windows SDK libraries needed at link time
    target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d11
        dxgi
        dxguid
        xinput1_4      # in every Windows 8+; xinput1_3 needs the old DirectX runtime
        winmm
        # Lets the fault handler resolve the host RIP back to the generated
        # function's symbol, i.e. to the guest function that faulted.
        dbghelp
    )
    if(MINGW)
        # MinGW's own runtime goes into the .exe: a posix-threads GCC (the
        # installer's WinLibs) otherwise links libgcc's pthread_* calls to
        # libwinpthread-1.dll, which only its bin\ folder has. System DLLs
        # (d3d11, the UCRT) stay imports; their libraries are import-only.
        target_link_options(${PROJECT_NAME} PRIVATE -static)
    endif()
else()
    # Native build. No -rdynamic: the fault handler names a place by its
    # offset in the executable (dladdr), and exporting every symbol would let
    # any guest function named like a libc one preempt it.
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS} m)
endif()

# MSVC-specific: large address aware, increase section limit for big generated files
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
        /LARGEADDRESSAWARE
    )
    # The generated code files can be very large. Increase object file section limit.
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

# ── Game-specific additions ─────────────────────────────────────
#
# Add any game-specific libraries, loaders, or custom renderers below.
# Examples:
#
# # Custom texture loader for your game's format
# target_sources(${PROJECT_NAME} PRIVATE src/texture_loader.c)
#
# # If your game uses RenderWare, you may need a RW bridge layer
# target_sources(${PROJECT_NAME} PRIVATE src/rw_bridge.c)
#
# # Additional compile definitions for debugging
# target_compile_definitions(${PROJECT_NAME} PRIVATE
#     ENABLE_ICALL_TRACE=1
#     VERBOSE_KERNEL_LOG=1
# )
