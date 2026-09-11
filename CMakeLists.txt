cmake_minimum_required(VERSION 3.20)
project(ISSDNative C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SNESRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/snesrecomp" CACHE PATH "Path to SNESRecomp")
set(SNESRECOMP_SDL_BACKEND "SDL2" CACHE STRING "SDL backend")

# On Linux/SteamOS SDL2 comes from the system (pkg-config finds it). On Windows
# there is no system package, so fall back to the usual scoop location - but
# only as a default the user can still override, never as a hard requirement.
if(WIN32 AND NOT CMAKE_PREFIX_PATH AND NOT DEFINED ENV{SDL2_DIR})
    set(_issd_sdl2_guess "C:/Users/sergi/scoop/apps/sdl2/current")
    if(EXISTS "${_issd_sdl2_guess}")
        set(CMAKE_PREFIX_PATH "${_issd_sdl2_guess}"
            CACHE PATH "Prefix used to locate SDL2")
    endif()
    unset(_issd_sdl2_guess)
endif()

include(${SNESRECOMP_ROOT}/runner/runner.cmake)

# One shared source list for every port (see cmake/issd_sources.cmake).
set(ISSD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/issd_sources.cmake)

add_executable(ISSDNative
    ${SNESRECOMP_RUNNER_SOURCES}
    ${ISSD_GENERATED_SOURCES}
    ${ISSD_GAME_SOURCES}
)

target_include_directories(ISSDNative PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/recomp/config"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative"
    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}
    "${SNESRECOMP_ROOT}/runner/src/desktop"
)

if(MSVC)
    target_compile_options(ISSDNative PRIVATE
        /W3
        /wd4018
        /wd4244
        /wd4267
        /wd4996
        /wd4146
    )
else()
    target_compile_options(ISSDNative PRIVATE
        -w
        -Wno-implicit-function-declaration
    )
endif()

# runner.cmake populates this with the libraries its own sources need: libm on
# every non-Windows host for cx4.c, plus ws2_32/dbghelp for trace builds. The
# project previously ignored it, which only showed up as an undefined cos() at
# link time on Linux because Windows folds libm into the CRT.
target_link_libraries(ISSDNative PRIVATE ${SNESRECOMP_RUNNER_LIBRARIES})

snesrecomp_target_sdl(ISSDNative)

# Without this deny set the SPC700 handshake wedges the reset vector and no
# window is created, so it has to travel with the executable, not just live in
# the source tree. See ApplyDefaultAotBootDenySet in ISSDNative/main.c.
add_custom_command(TARGET ISSDNative POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/recomp/aot_boot_deny.txt"
        $<TARGET_FILE_DIR:ISSDNative>
)

if(WIN32)
    # comdlg32 is no longer used for the ROM picker (the runner's
    # cross-platform snesrecomp_pick_rom_file covers every platform), but winmm
    # and ws2_32 remain runner dependencies on Windows.
    target_link_libraries(ISSDNative PRIVATE winmm ws2_32)

    # runner.cmake copies SDL2.dll itself when SDL2 is found as an imported
    # shared target. This covers the plain SDL2_LIBRARIES case, where there is
    # no target to copy from. Silently skipped when the DLL is not where the
    # prefix suggests, rather than baking one machine's layout into the build.
    if(NOT TARGET SDL2::SDL2 AND CMAKE_PREFIX_PATH)
        set(_issd_sdl2_dll "${CMAKE_PREFIX_PATH}/lib/SDL2.dll")
        if(EXISTS "${_issd_sdl2_dll}")
            add_custom_command(TARGET ISSDNative POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_issd_sdl2_dll}" $<TARGET_FILE_DIR:ISSDNative>)
        endif()
        unset(_issd_sdl2_dll)
    endif()
endif()
