cmake_minimum_required(VERSION 3.20)
project(igi C)

set(CMAKE_C_STANDARD 11)

set(IGI_DIRS "" CACHE STRING "Build only these src subfolders, for working on one area")

if(IGI_DIRS)
    file(GLOB IGI_SOURCES CONFIGURE_DEPENDS src/*.c)
    foreach(dir ${IGI_DIRS})
        file(GLOB_RECURSE dir_sources CONFIGURE_DEPENDS src/${dir}/*.c)
        list(APPEND IGI_SOURCES ${dir_sources})
    endforeach()
else()
    file(GLOB_RECURSE IGI_SOURCES CONFIGURE_DEPENDS src/*.c)
endif()

list(FILTER IGI_SOURCES EXCLUDE REGEX "/src/(exe/|dllmain\\.c)")

add_library(igi OBJECT ${IGI_SOURCES})
target_include_directories(igi PUBLIC src)
target_compile_options(igi PUBLIC -Wall -O2 -fno-strict-aliasing -fno-math-errno -march=i686
    -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

add_library(dinput SHARED $<TARGET_OBJECTS:igi> src/dllmain.c src/dinput.def)
set_target_properties(dinput PROPERTIES PREFIX "")
target_link_libraries(dinput PRIVATE igi)
target_link_options(dinput PRIVATE -static -static-libgcc -Wl,--image-base,0x10000000)

# standalone build: needs only the player's IGI.exe for its data (src/exe/main.c)
add_executable(igidecomp WIN32 $<TARGET_OBJECTS:igi> src/exe/main.c)
target_link_libraries(igidecomp PRIVATE igi)
target_link_options(igidecomp PRIVATE -static -static-libgcc -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase
    -Wl,--section-start=.igi=0x401000 -Wl,--section-start=.text=0xC35000 -Wl,--wrap=crt_atof)
