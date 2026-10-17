cmake_minimum_required(VERSION 3.20)
project(wheresmybuddy LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Targets Windows. On Linux use -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()

# Azure app (client) ID for Microsoft sign-in, baked into the default
# WheresMyBuddy.ini. Public client ID, not a secret.
set(WMB_CLIENT_ID "" CACHE STRING "Azure app (client) ID used for Xbox sign-in")

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# FH2 Mod Loader (identical to the one FH2 Universal Radio ships).
add_library(fh2loader SHARED loader/loader.cpp)
if(NOT MSVC)
    target_sources(fh2loader PRIVATE loader/d3dcompiler_47.def)
endif()
target_include_directories(fh2loader PRIVATE loader)
target_link_libraries(fh2loader PRIVATE user32)
set_target_properties(fh2loader PROPERTIES OUTPUT_NAME d3dcompiler_47 PREFIX "")

# The mod: fh2-mods\WheresMyBuddy.dll
add_library(wheresmybuddy SHARED src/dll_main.cpp src/buddy.cpp src/xbox.cpp src/util.cpp)
target_include_directories(wheresmybuddy PRIVATE src third_party/nlohmann)
target_compile_definitions(wheresmybuddy PRIVATE WMB_DEFAULT_CLIENT_ID="${WMB_CLIENT_ID}")
target_link_libraries(wheresmybuddy PRIVATE winhttp crypt32 shell32 user32)
set_target_properties(wheresmybuddy PROPERTIES OUTPUT_NAME WheresMyBuddy PREFIX "")

option(WMB_TESTS "Build the offline test" OFF)
if(WMB_TESTS)
    add_executable(wmb_test tests/wmb_test.cpp src/buddy.cpp src/util.cpp)
    target_include_directories(wmb_test PRIVATE src third_party/nlohmann)
    target_compile_definitions(wmb_test PRIVATE WMB_DEFAULT_CLIENT_ID="${WMB_CLIENT_ID}")
    target_link_libraries(wmb_test PRIVATE winhttp)
endif()
