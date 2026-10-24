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
set(IGI_CFLAGS -Wall -O2 -fno-strict-aliasing -fno-math-errno -march=i686 -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

add_library(dinput SHARED ${IGI_SOURCES} src/dllmain.c src/dinput.def)
set_target_properties(dinput PROPERTIES PREFIX "")
target_include_directories(dinput PRIVATE src)
target_compile_options(dinput PRIVATE ${IGI_CFLAGS})
target_link_options(dinput PRIVATE -static -static-libgcc -Wl,--image-base,0x10000000)

# Standalone igidecomp.exe (src/exe): ORIG calls link straight to the rewrite (src/igi.h) and
# msvcrt.dll replaces the C runtime rewrites in terrdraw/crt.c. Linked at 0x400000 with the
# empty .igi section over IGI.exe's image, which it fills with IGI.exe's data at startup.
if(NOT IGI_DIRS)
    set(IGI_EXE_SOURCES ${IGI_SOURCES})
    list(FILTER IGI_EXE_SOURCES EXCLUDE REGEX "/src/terrdraw/crt\\.c$")
    file(GLOB IGI_EXE_MAIN CONFIGURE_DEPENDS src/exe/*.c)
    add_executable(igidecomp WIN32 ${IGI_EXE_SOURCES} ${IGI_EXE_MAIN})
    target_include_directories(igidecomp PRIVATE src)
    target_compile_definitions(igidecomp PRIVATE IGI_STANDALONE)
    target_compile_options(igidecomp PRIVATE ${IGI_CFLAGS})
    target_link_libraries(igidecomp PRIVATE ddraw dsound vfw32)
    target_link_options(igidecomp PRIVATE -static -static-libgcc -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase
        -Wl,--section-start=.igi=0x401000 -Wl,--section-start=.text=0xC35000)
endif()
