cmake_minimum_required(VERSION 3.20)
project(igi C)

set(CMAKE_C_STANDARD 11)

file(GLOB_RECURSE IGI_SOURCES CONFIGURE_DEPENDS src/*.c)

add_library(dinput SHARED ${IGI_SOURCES} src/dinput.def)
set_target_properties(dinput PROPERTIES PREFIX "")
target_include_directories(dinput PRIVATE src)
target_compile_options(dinput PRIVATE -Wall -O2 -fno-strict-aliasing -march=i686)
target_link_options(dinput PRIVATE -static -static-libgcc -Wl,--image-base,0x10000000)
