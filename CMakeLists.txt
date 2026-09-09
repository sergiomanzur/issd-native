cmake_minimum_required(VERSION 3.20)
project(ISSDNative C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SNESRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/snesrecomp" CACHE PATH "Path to SNESRecomp")
set(SNESRECOMP_SDL_BACKEND "SDL2" CACHE STRING "SDL backend")
set(CMAKE_PREFIX_PATH "C:/Users/sergi/scoop/apps/sdl2/current" CACHE PATH "Prefix for SDL2")

include(${SNESRECOMP_ROOT}/runner/runner.cmake)

file(GLOB GENERATED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/recomp/generated/*.c"
)

set(GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/issd_bridge.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/issd_widescreen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/issd_config.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/issd_save.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/issd_mod.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/issd_menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ISSDNative/issd_decompress.c"
)

add_executable(ISSDNative
    ${SNESRECOMP_RUNNER_SOURCES}
    ${GENERATED_SOURCES}
    ${GAME_SOURCES}
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
    target_link_libraries(ISSDNative PRIVATE winmm ws2_32 comdlg32)
    # Copy SDL2.dll next to the target executable
    add_custom_command(TARGET ISSDNative POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Users/sergi/scoop/apps/sdl2/current/lib/SDL2.dll"
            $<TARGET_FILE_DIR:ISSDNative>
    )
endif()
