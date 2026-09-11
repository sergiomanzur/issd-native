# Native half of the Android app.
#
# Android differs from the desktop targets in two ways that matter here:
#   - the artifact is libmain.so, not an executable. SDLActivity dlopen()s it
#     and calls SDL_main.
#   - SDL2 is compiled from source rather than linked from the system, because
#     SDL owns the Java Activity that drives the Android window and lifecycle.
#
# The source list itself is shared with the desktop build, so a file added
# there is picked up here and the two cannot drift apart.
cmake_minimum_required(VERSION 3.22)
project(ISSDNativeAndroid C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(ISSD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(SNESRECOMP_ROOT "${ISSD_ROOT}/deps/snesrecomp")
set(SDL_ROOT        "${ISSD_ROOT}/deps/SDL2")

if(NOT EXISTS "${SDL_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "deps/SDL2 is missing. Run scripts/fetch-sdl2.sh first -- Android "
        "needs SDL2 from source for its Java activity glue.")
endif()

# SDL2's own Android build. SDL2 and hidapi come out as shared libraries that
# the APK packages alongside libmain.so.
add_subdirectory("${SDL_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/SDL2" EXCLUDE_FROM_ALL)

include(${SNESRECOMP_ROOT}/runner/runner.cmake)
include(${ISSD_ROOT}/cmake/issd_sources.cmake)

add_library(main SHARED
    ${SNESRECOMP_RUNNER_SOURCES}
    ${ISSD_GENERATED_SOURCES}
    ${ISSD_GAME_SOURCES}
)

target_include_directories(main PRIVATE
    "${ISSD_ROOT}/recomp/config"
    "${ISSD_ROOT}/ISSDNative"
    ${SNESRECOMP_RUNNER_INCLUDE_DIRS}
    "${SNESRECOMP_ROOT}/runner/src/desktop"
    "${SDL_ROOT}/include"
)

# SDL_MAIN_HANDLED is right for a desktop executable that owns its own main().
# On Android SDL owns the entry point: SDL_main.h renames main() to SDL_main(),
# which SDLActivity calls. Defining SDL_MAIN_HANDLED here would leave the app
# with no entry point at all.
target_compile_definitions(main PRIVATE
    SNESRECOMP_SDL3=0
    SNESRECOMP_ENABLE_MODS=0
    ISSD_ANDROID=1
)

target_compile_options(main PRIVATE -w -Wno-implicit-function-declaration)

target_link_libraries(main PRIVATE
    SDL2
    ${SNESRECOMP_RUNNER_LIBRARIES}
    android
    log
    GLESv2
    EGL
)
