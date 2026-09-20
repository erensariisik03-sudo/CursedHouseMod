cmake_minimum_required(VERSION 3.18.1)

project(multiplayermod LANGUAGES C CXX ASM)

if (NOT ANDROID)
    message(FATAL_ERROR "multiplayermod is an Android NDK project. Configure it with android.toolchain.cmake.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(multiplayermod SHARED
    src/main.cpp
    src/arm_hook.cpp
)

target_include_directories(multiplayermod PRIVATE include)

find_library(log-lib log REQUIRED)
find_library(dl-lib dl REQUIRED)

# No external Substrate/Dobby dependency is required for the ARM32 build.
target_link_libraries(multiplayermod PRIVATE ${log-lib} ${dl-lib})

if (ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_definitions(multiplayermod PRIVATE CH_ARM32=1)
else()
    message(FATAL_ERROR "This project currently supports ARM32/armeabi-v7a only.")
endif()

message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "Android platform: ${ANDROID_PLATFORM}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
