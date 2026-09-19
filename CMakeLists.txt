cmake_minimum_required(VERSION 3.18.1)

project(cursed_house_native_keyboard LANGUAGES CXX)

if (NOT ANDROID)
    message(FATAL_ERROR "This project must be configured with the Android NDK toolchain.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI armeabi-v7a CACHE STRING "Android ABI" FORCE)
endif()

add_library(cursedhouse_native_keyboard SHARED
    src/main.cpp
)

target_include_directories(cursedhouse_native_keyboard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_library(log-lib log REQUIRED)
find_library(dl-lib dl REQUIRED)

find_package(Threads REQUIRED)

target_link_libraries(cursedhouse_native_keyboard PRIVATE
    ${log-lib}
    ${dl-lib}
    Threads::Threads
)

message(STATUS "ABI: ${ANDROID_ABI}")
message(STATUS "Platform: ${ANDROID_PLATFORM}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
