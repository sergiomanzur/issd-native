# Source list shared by every port.
#
# Kept in one place so the desktop and Android builds cannot drift apart: a new
# file added here is picked up by Windows, Linux, SteamOS and Android at once.
# ISSD_ROOT must be set by the caller.

if(NOT DEFINED ISSD_ROOT)
    message(FATAL_ERROR "issd_sources.cmake: set ISSD_ROOT before including")
endif()

file(GLOB ISSD_GENERATED_SOURCES CONFIGURE_DEPENDS
    "${ISSD_ROOT}/recomp/generated/*.c"
)
if(NOT ISSD_GENERATED_SOURCES)
    message(FATAL_ERROR
        "No recompiled sources in ${ISSD_ROOT}/recomp/generated -- "
        "the cartridge translation units are missing.")
endif()

set(ISSD_GAME_SOURCES
    "${ISSD_ROOT}/ISSDNative/main.c"
    "${ISSD_ROOT}/ISSDNative/issd_bridge.c"
    "${ISSD_ROOT}/ISSDNative/issd_widescreen.c"
    "${ISSD_ROOT}/ISSDNative/issd_pose_history.c"
    "${ISSD_ROOT}/ISSDNative/issd_config.c"
    "${ISSD_ROOT}/ISSDNative/issd_save.c"
    "${ISSD_ROOT}/ISSDNative/issd_mod.c"
    "${ISSD_ROOT}/ISSDNative/issd_mod_rom.c"
    "${ISSD_ROOT}/ISSDNative/issd_menu.c"
    "${ISSD_ROOT}/ISSDNative/issd_decompress.c"
    "${ISSD_ROOT}/ISSDNative/issd_audio.c"
    "${ISSD_ROOT}/ISSDNative/issd_touch.c"
    "${ISSD_ROOT}/ISSDNative/issd_script.c"
    "${ISSD_ROOT}/ISSDNative/issd_android.c"
)
