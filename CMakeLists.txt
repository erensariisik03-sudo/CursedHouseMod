cmake_minimum_required(VERSION 3.18.1)

project(multiplayermod LANGUAGES CXX)

# Android-only shared library.
if (NOT ANDROID)
    message(FATAL_ERROR "multiplayermod is an Android NDK project. Configure it with android.toolchain.cmake.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to the ABI implied by the source's Thumb-mode hook address.
if (NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI armeabi-v7a CACHE STRING "Android ABI" FORCE)
endif()

add_library(multiplayermod SHARED
    src/main.cpp
)

target_include_directories(multiplayermod
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Android logging (__android_log_print).
find_library(log-lib log REQUIRED)
find_library(dl-lib dl REQUIRED)

target_link_libraries(multiplayermod
    PRIVATE
        ${log-lib}
        ${dl-lib}
)

# MSHookFunction gibi çalışma zamanında yüklenecek semboller için 
# linker'ın 'undefined symbol' hatası vermesini engeller:
target_link_options(multiplayermod PRIVATE -Wl,-z,undefs)

# Helpful ABI diagnostics.
message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "Android platform: ${ANDROID_PLATFORM}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
