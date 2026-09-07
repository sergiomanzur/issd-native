cmake_minimum_required(VERSION 3.16)
project(retcomm_rbengine VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RBE_BUILD_TESTS "Build unit tests" ON)

# recomp-net: sibling checkout, RECOMP_NET_ROOT, or FetchContent.
set(RECOMP_NET_ROOT "" CACHE PATH "Path to recomp-net sources")
if(NOT RECOMP_NET_ROOT)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../recomp-net/CMakeLists.txt")
    set(RECOMP_NET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../recomp-net")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/recomp-net/CMakeLists.txt")
    set(RECOMP_NET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/recomp-net")
  endif()
endif()

if(NOT TARGET recomp_net)
  if(NOT RECOMP_NET_ROOT OR NOT EXISTS "${RECOMP_NET_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
      "retcomm_rbengine requires recomp-net. Set -DRECOMP_NET_ROOT=... or "
      "place a checkout at ../recomp-net or external/recomp-net.")
  endif()
  set(RNET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(RNET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(RNET_ENABLE_ICE OFF CACHE BOOL "" FORCE)
  add_subdirectory("${RECOMP_NET_ROOT}" "${CMAKE_BINARY_DIR}/recomp-net")
endif()

set(RBE_SOURCES
  src/input/rbe_hash_confirm.c
  src/input/rbe_input_hist.c
  src/snap/rbe_snap_ring.c
  src/sched/rbe_sched.c
  src/util/rbe_mono_ms.c
)

add_library(retcomm_rbengine STATIC ${RBE_SOURCES})
add_library(retcomm_rbengine::retcomm_rbengine ALIAS retcomm_rbengine)

target_include_directories(retcomm_rbengine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(retcomm_rbengine PUBLIC recomp_net)

target_compile_definitions(retcomm_rbengine PRIVATE
  $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)
if(UNIX)
  target_compile_definitions(retcomm_rbengine PRIVATE
    _POSIX_C_SOURCE=200809L
    _DEFAULT_SOURCE
  )
endif()

if(MSVC)
  target_compile_options(retcomm_rbengine PRIVATE /W4)
else()
  target_compile_options(retcomm_rbengine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RBE_BUILD_TESTS)
  enable_testing()

  add_executable(hash_confirm_test tests/hash_confirm_test.c)
  target_link_libraries(hash_confirm_test PRIVATE retcomm_rbengine)
  add_test(NAME hash_confirm_test COMMAND hash_confirm_test)

  add_executable(input_hist_test tests/input_hist_test.c)
  target_link_libraries(input_hist_test PRIVATE retcomm_rbengine)
  add_test(NAME input_hist_test COMMAND input_hist_test)

  add_executable(snap_ring_test tests/snap_ring_test.c)
  target_link_libraries(snap_ring_test PRIVATE retcomm_rbengine)
  add_test(NAME snap_ring_test COMMAND snap_ring_test)

  add_executable(rb_post_test tests/rb_post_test.c)
  target_link_libraries(rb_post_test PRIVATE retcomm_rbengine)
  add_test(NAME rb_post_test COMMAND rb_post_test)

  add_executable(sched_smoke_test tests/sched_smoke_test.c)
  target_link_libraries(sched_smoke_test PRIVATE retcomm_rbengine)
  add_test(NAME sched_smoke_test COMMAND sched_smoke_test)
endif()

install(TARGETS retcomm_rbengine ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY include/retcomm_rbengine DESTINATION include)
