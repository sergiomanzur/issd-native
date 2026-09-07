cmake_minimum_required(VERSION 3.20)
project(International_Superstar_Soccer_Deluxe_USARecomp C)
set(CMAKE_C_STANDARD 11)

file(GLOB GENERATED_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.c")
add_library(snesrecomp_game STATIC ${GENERATED_SOURCES})
target_include_directories(snesrecomp_game PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/config"
  "${CMAKE_CURRENT_SOURCE_DIR}/snesrecomp/runner/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/snesrecomp/runner/src/snes")
if(NOT MSVC)
  target_compile_options(snesrecomp_game PRIVATE
    -w -Wno-implicit-function-declaration)
endif()
