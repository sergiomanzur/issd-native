find_path(SDL2_INCLUDE_DIR
    NAMES SDL.h
    PATH_SUFFIXES SDL2 include/SDL2 include
    PATHS
        "${SDL2_ROOT}"
        "C:/Users/sergi/scoop/apps/sdl2/current"
)

find_library(SDL2_LIBRARY
    NAMES SDL2
    PATH_SUFFIXES lib
    PATHS
        "${SDL2_ROOT}"
        "C:/Users/sergi/scoop/apps/sdl2/current"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
    REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR
)

if(SDL2_FOUND)
    set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
        )
    endif()
endif()
