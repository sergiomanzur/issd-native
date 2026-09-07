# FindLibjuice.cmake — locate libjuice (system install or RNET_LIBJUICE_ROOT).
#
# Imported target: Libjuice::Libjuice
# Variables: Libjuice_FOUND, Libjuice_INCLUDE_DIRS, Libjuice_LIBRARIES

set(_rnet_juice_hints)
if(DEFINED RNET_LIBJUICE_ROOT AND NOT RNET_LIBJUICE_ROOT STREQUAL "")
  list(APPEND _rnet_juice_hints "${RNET_LIBJUICE_ROOT}")
endif()
if(DEFINED ENV{RNET_LIBJUICE_ROOT})
  list(APPEND _rnet_juice_hints "$ENV{RNET_LIBJUICE_ROOT}")
endif()
list(APPEND _rnet_juice_hints
  "${CMAKE_SOURCE_DIR}/third_party/libjuice"
  "${CMAKE_SOURCE_DIR}/../libjuice"
)

find_path(Libjuice_INCLUDE_DIR
  NAMES juice/juice.h
  HINTS ${_rnet_juice_hints}
  PATH_SUFFIXES include
)

find_library(Libjuice_LIBRARY
  NAMES juice juice-static libjuice
  HINTS ${_rnet_juice_hints}
  PATH_SUFFIXES lib lib64 build build/lib
)

# Prefer building from a vendored source tree when headers exist but no library yet.
if(NOT Libjuice_LIBRARY)
  foreach(_hint IN LISTS _rnet_juice_hints)
    if(EXISTS "${_hint}/CMakeLists.txt" AND EXISTS "${_hint}/include/juice/juice.h")
      set(Libjuice_INCLUDE_DIR "${_hint}/include" CACHE PATH "libjuice include" FORCE)
      if(NOT TARGET juice AND NOT TARGET juice-static)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(NO_TESTS ON CACHE BOOL "" FORCE)
        set(NO_SERVER ON CACHE BOOL "" FORCE)
        set(USE_NETTLE OFF CACHE BOOL "" FORCE)
        set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${_hint}" "${CMAKE_BINARY_DIR}/_deps/libjuice" EXCLUDE_FROM_ALL)
      endif()
      if(TARGET juice-static)
        set(Libjuice_LIBRARY juice-static)
      elseif(TARGET juice)
        set(Libjuice_LIBRARY juice)
      endif()
      break()
    endif()
  endforeach()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libjuice
  REQUIRED_VARS Libjuice_INCLUDE_DIR Libjuice_LIBRARY
)

if(Libjuice_FOUND)
  set(Libjuice_INCLUDE_DIRS "${Libjuice_INCLUDE_DIR}")
  set(Libjuice_LIBRARIES "${Libjuice_LIBRARY}")
  if(NOT TARGET Libjuice::Libjuice)
    if(TARGET "${Libjuice_LIBRARY}")
      add_library(Libjuice::Libjuice ALIAS ${Libjuice_LIBRARY})
    else()
      add_library(Libjuice::Libjuice UNKNOWN IMPORTED)
      set_target_properties(Libjuice::Libjuice PROPERTIES
        IMPORTED_LOCATION "${Libjuice_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libjuice_INCLUDE_DIR}"
      )
    endif()
  endif()
endif()

mark_as_advanced(Libjuice_INCLUDE_DIR Libjuice_LIBRARY)
