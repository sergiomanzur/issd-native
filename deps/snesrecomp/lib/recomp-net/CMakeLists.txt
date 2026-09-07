cmake_minimum_required(VERSION 3.16)
project(recomp_net VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RNET_ENABLE_ICE "Build libjuice ICE transport" OFF)
option(RNET_ICE_BUNDLE_STATIC
  "Fetch/build static libjuice into recomp_net (portable; no system .so)" ON)
option(RNET_ICE_FORCE_TURN
  "Testing: only emit/accept ICE typ relay candidates (requires TURN)" OFF)
set(RNET_LIBJUICE_ROOT "" CACHE PATH
  "Optional path to libjuice sources (skip FetchContent when set)")
option(RNET_BUILD_EXAMPLES "Build example programs" ON)
option(RNET_BUILD_TESTS "Build unit tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(RNET_CORE_SOURCES
  src/platform/rnet_address.c
  src/platform/rnet_platform.c
  src/platform/rnet_stun.c
  src/protocol/rnet_protocol.c
  src/input/rnet_rings.c
  src/input/rnet_input_contract.c
  src/rollback/rnet_rollback.c
  src/lobby/rnet_lan_lobby.c
  src/lobby/rnet_lan_direct.c
  src/lobby/rnet_lan_beacon.c
  src/lobby/rnet_rtt_probe.c
  src/lobby/rnet_ice_rtt.c
  src/lobby/rnet_ice_xfer.c
  src/transport/rnet_transport.c
  src/session/rnet_session.c
  src/ice/rnet_ice_state.c
  src/ice/rnet_ice_agent.c
)

add_library(recomp_net STATIC ${RNET_CORE_SOURCES})
add_library(recomp_net::recomp_net ALIAS recomp_net)

target_include_directories(recomp_net
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(recomp_net PRIVATE $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>)
if(UNIX)
  target_compile_definitions(recomp_net PRIVATE _POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
  # Darwin gates BSD extensions in <net/if.h> behind
  # `!_POSIX_C_SOURCE || _DARWIN_C_SOURCE`. Strict POSIX above hides IFF_UP
  # (rnet_address.c). _DEFAULT_SOURCE is glibc-only and does nothing here.
  if(APPLE)
    target_compile_definitions(recomp_net PRIVATE _DARWIN_C_SOURCE)
  endif()
endif()

if(WIN32)
  target_link_libraries(recomp_net PUBLIC ws2_32 iphlpapi bcrypt)
endif()

if(RNET_ENABLE_ICE)
  # Portable game embeds: prefer a static juice linked into recomp_net so Linux
  # binaries do not depend on a distro libjuice.so at runtime. Opt out with
  # -DRNET_ICE_BUNDLE_STATIC=OFF to use find_package(Libjuice) (shared OK).
  set(_rnet_juice_ok FALSE)
  if(NOT RNET_ICE_BUNDLE_STATIC)
    find_package(Libjuice QUIET)
    if(Libjuice_FOUND)
      set(_rnet_juice_ok TRUE)
    endif()
  endif()
  # Offline / vendored tree (setup CI can pre-seed this to skip network).
  if(NOT _rnet_juice_ok AND EXISTS
      "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjuice/CMakeLists.txt")
    message(STATUS "Libjuice: using vendored third_party/libjuice")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(NO_SERVER ON CACHE BOOL "" FORCE)
    set(USE_NETTLE OFF CACHE BOOL "" FORCE)
    set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/libjuice"
                     "${CMAKE_BINARY_DIR}/third_party-libjuice")
    set(_rnet_juice_ok TRUE)
  endif()
  if(NOT _rnet_juice_ok AND RNET_LIBJUICE_ROOT AND EXISTS
      "${RNET_LIBJUICE_ROOT}/CMakeLists.txt")
    message(STATUS "Libjuice: using RNET_LIBJUICE_ROOT=${RNET_LIBJUICE_ROOT}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(NO_SERVER ON CACHE BOOL "" FORCE)
    set(USE_NETTLE OFF CACHE BOOL "" FORCE)
    set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${RNET_LIBJUICE_ROOT}"
                     "${CMAKE_BINARY_DIR}/rnet-libjuice-root")
    set(_rnet_juice_ok TRUE)
  endif()
  if(NOT _rnet_juice_ok)
    include(FetchContent)
    # URL archive (not GIT_REPOSITORY): cmake's file(DOWNLOAD) avoids system
    # git-remote-https, which breaks under RetComM AppImage LD_LIBRARY_PATH
    # (curl_global_trace / CURL_OPENSSL_4 symbol mismatch).
    #
    # CMP0135 NEW + DOWNLOAD_EXTRACT_TIMESTAMP: stamp extracted files at
    # extract time, not archive mtimes. Archive timestamps (often far in the
    # past, occasionally ahead of a skewed host clock) make Ninja treat
    # build.ninja as forever dirty ("still dirty after 100 tries") on Windows
    # Hub builds that FetchContent libjuice into the game build tree.
    message(STATUS
      "Libjuice: bundling static juice via FetchContent URL "
      "(RNET_ICE_BUNDLE_STATIC=${RNET_ICE_BUNDLE_STATIC})")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(NO_SERVER ON CACHE BOOL "" FORCE)
    set(USE_NETTLE OFF CACHE BOOL "" FORCE)
    set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    if(POLICY CMP0135)
      cmake_policy(SET CMP0135 NEW)
    endif()
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
      FetchContent_Declare(
        libjuice
        URL https://github.com/paullouisageneau/libjuice/archive/refs/tags/v1.7.2.tar.gz
        URL_HASH SHA256=75159867c4a5a689a6559e11aa0d30c9eba12ce73a4ae3d898b521467e1f635d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
      )
    else()
      FetchContent_Declare(
        libjuice
        URL https://github.com/paullouisageneau/libjuice/archive/refs/tags/v1.7.2.tar.gz
        URL_HASH SHA256=75159867c4a5a689a6559e11aa0d30c9eba12ce73a4ae3d898b521467e1f635d
      )
    endif()
    FetchContent_MakeAvailable(libjuice)
    set(_rnet_juice_ok TRUE)
  endif()
  if(_rnet_juice_ok)
    # Parent CMAKE_C_EXTENSIONS=OFF compiles juice as -std=c11.
    # Unix: _GNU_SOURCE exposes getaddrinfo. Windows/MinGW: that same
    # macro plus __STRICT_ANSI__ skips IN_ADDR in inaddr.h, then
    # mstcpip.h (via ws2tcpip.h) fails; libjuice also needs gnu11 for
    # <sys/time.h>. C_EXTENSIONS ON alone is not enough under RetComM
    # cmake-clang-v1 — toolchain / CMAKE_C_FLAGS often keep -std=c11, so
    # force -std=gnu11 on the juice targets for Clang/GNU on Windows.
    foreach(_rnet_juice IN ITEMS juice-static juice)
      if(TARGET ${_rnet_juice})
        if(UNIX)
          target_compile_definitions(${_rnet_juice} PRIVATE _GNU_SOURCE)
        endif()
        set_target_properties(${_rnet_juice} PROPERTIES
          C_STANDARD 11
          C_STANDARD_REQUIRED ON
          C_EXTENSIONS ON)
        if(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
          target_compile_options(${_rnet_juice} PRIVATE -std=gnu11)
        endif()
      endif()
    endforeach()
    if(NOT TARGET Libjuice::Libjuice)
      if(TARGET juice-static)
        add_library(Libjuice::Libjuice ALIAS juice-static)
      elseif(TARGET juice)
        add_library(Libjuice::Libjuice ALIAS juice)
      else()
        message(FATAL_ERROR
          "RNET_ENABLE_ICE=ON but libjuice was not found and FetchContent did not "
          "produce a juice target. Install libjuice, set RNET_LIBJUICE_ROOT, "
          "vendor sources under third_party/libjuice, or enable network for "
          "FetchContent.")
      endif()
    endif()
  endif()
  target_compile_definitions(recomp_net PUBLIC RNET_ENABLE_ICE=1)
  if(RNET_ICE_FORCE_TURN)
    target_compile_definitions(recomp_net PUBLIC RNET_ICE_FORCE_TURN=1)
    message(STATUS "recomp_net: RNET_ICE_FORCE_TURN — relay-only ICE candidates")
  endif()
  target_link_libraries(recomp_net PUBLIC Libjuice::Libjuice)
  if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(recomp_net PUBLIC Threads::Threads)
  endif()
  # When ICE is off, session uses inline stubs from rnet_ice_internal.h.
endif()

if(MSVC)
  target_compile_options(recomp_net PRIVATE /W4)
else()
  target_compile_options(recomp_net PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Examples ----
if(RNET_BUILD_EXAMPLES)
  add_executable(lan_delay_2p examples/lan_delay_2p/main.c)
  target_link_libraries(lan_delay_2p PRIVATE recomp_net)

  add_executable(ice_manual_signaling examples/ice_manual_signaling/main.c)
  target_link_libraries(ice_manual_signaling PRIVATE recomp_net)
endif()

# ---- Tests ----
if(RNET_BUILD_TESTS)
  enable_testing()

  add_executable(ring_admit_test tests/ring_admit_test.c)
  target_link_libraries(ring_admit_test PRIVATE recomp_net)
  target_include_directories(ring_admit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME ring_admit_test COMMAND ring_admit_test)

  add_executable(wire_map_test tests/wire_map_test.c)
  target_link_libraries(wire_map_test PRIVATE recomp_net)
  add_test(NAME wire_map_test COMMAND wire_map_test)

  add_executable(hostport_test tests/hostport_test.c)
  target_link_libraries(hostport_test PRIVATE recomp_net)
  target_include_directories(hostport_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME hostport_test COMMAND hostport_test)

  add_executable(lan_lobby_test tests/lan_lobby_test.c)
  target_link_libraries(lan_lobby_test PRIVATE recomp_net)
  add_test(NAME lan_lobby_test COMMAND lan_lobby_test)

  add_executable(udp_port_test tests/udp_port_test.c)
  target_link_libraries(udp_port_test PRIVATE recomp_net)
  target_include_directories(udp_port_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME udp_port_test COMMAND udp_port_test)

  add_executable(address_test tests/address_test.c)
  target_link_libraries(address_test PRIVATE recomp_net)
  target_include_directories(address_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME address_test COMMAND address_test)

  add_executable(stun_test tests/stun_test.c)
  target_link_libraries(stun_test PRIVATE recomp_net)
  target_include_directories(stun_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME stun_test COMMAND stun_test)

  add_executable(session_state_test tests/session_state_test.c)
  target_link_libraries(session_state_test PRIVATE recomp_net)
  add_test(NAME session_state_test COMMAND session_state_test)

  add_executable(input_contract_test tests/input_contract_test.c)
  target_link_libraries(input_contract_test PRIVATE recomp_net)
  add_test(NAME input_contract_test COMMAND input_contract_test)

  add_executable(rollback_episode_test tests/rollback_episode_test.c)
  target_link_libraries(rollback_episode_test PRIVATE recomp_net)
  add_test(NAME rollback_episode_test COMMAND rollback_episode_test)

  add_executable(rb_wire_test tests/rb_wire_test.c)
  target_link_libraries(rb_wire_test PRIVATE recomp_net)
  target_include_directories(rb_wire_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME rb_wire_test COMMAND rb_wire_test)
endif()

install(TARGETS recomp_net ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY include/recomp_net DESTINATION include)
