cmake_minimum_required(VERSION 3.20)
# Bump VERSION before tagging a release (the tag build checks they match).
project(wheresmybuddy VERSION 1.7.0 LANGUAGES C CXX RC)

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
set(WMB_CLIENT_ID "12864041-3b5d-477c-b77c-b058dbb68ba0" CACHE STRING "Azure app (client) ID used for Xbox sign-in")

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# FH2ML's v4 mod API (FH2ML_v4.h, MIT), vendored from github.com/Refilewski/FH2ML.
# FH2ML loads the mod from mods\wheresmybuddy and provides every service it uses.
set(FH2ML_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/fh2ml)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/fh2_version.h)
# mods\wheresmybuddy\mod.json, with the same version as the DLL reports (WMB_VERSION).
configure_file(cmake/mod.json.in ${CMAKE_BINARY_DIR}/mod.json @ONLY)

# The mod: mods\wheresmybuddy\WheresMyBuddy.dll
add_library(wheresmybuddy SHARED src/dll_main.cpp src/buddy.cpp src/xbox.cpp src/signin_browser.cpp src/profile.cpp
            src/championship.cpp src/aiplayers.cpp src/platform_link.cpp src/util.cpp src/version.rc)
target_include_directories(wheresmybuddy PRIVATE src include third_party/nlohmann ${FH2ML_INCLUDE_DIR}
                           ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(wheresmybuddy PRIVATE WMB_DEFAULT_CLIENT_ID="${WMB_CLIENT_ID}" WMB_VERSION="${PROJECT_VERSION}")
target_link_libraries(wheresmybuddy PRIVATE winhttp crypt32 shell32 user32 ws2_32 bcrypt)
set_target_properties(wheresmybuddy PROPERTIES OUTPUT_NAME WheresMyBuddy PREFIX "")

option(WMB_TESTS "Build the offline test" OFF)
if(WMB_TESTS)
    add_executable(wmb_test tests/wmb_test.cpp src/buddy.cpp src/aiplayers.cpp src/platform_link.cpp src/util.cpp)
    target_include_directories(wmb_test PRIVATE src include third_party/nlohmann ${FH2ML_INCLUDE_DIR})
    target_compile_definitions(wmb_test PRIVATE WMB_DEFAULT_CLIENT_ID="${WMB_CLIENT_ID}" WMB_VERSION="${PROJECT_VERSION}")
    target_link_libraries(wmb_test PRIVATE winhttp)
endif()
if(WMB_TESTS)
    add_executable(browser_test tests/browser_test.cpp src/signin_browser.cpp src/util.cpp)
    target_include_directories(browser_test PRIVATE src include)
    target_link_libraries(browser_test PRIVATE winhttp ws2_32 bcrypt shell32 user32)
endif()
if(WMB_TESTS)
    add_executable(profile_test tests/profile_test.cpp src/profile.cpp src/buddy.cpp src/aiplayers.cpp src/platform_link.cpp src/util.cpp)
    target_include_directories(profile_test PRIVATE src include third_party/nlohmann ${FH2ML_INCLUDE_DIR})
    target_compile_definitions(profile_test PRIVATE WMB_DEFAULT_CLIENT_ID="${WMB_CLIENT_ID}" WMB_VERSION="${PROJECT_VERSION}")
    target_link_libraries(profile_test PRIVATE winhttp)
endif()
if(WMB_TESTS)
    add_executable(championship_test tests/championship_test.cpp src/championship.cpp src/platform_link.cpp src/util.cpp)
    target_include_directories(championship_test PRIVATE src include ${FH2ML_INCLUDE_DIR})
    target_compile_definitions(championship_test PRIVATE WMB_VERSION="${PROJECT_VERSION}")
    target_link_options(championship_test PRIVATE -municode -Wl,--image-base=0x10000000)
    target_link_libraries(championship_test PRIVATE winhttp)
endif()
