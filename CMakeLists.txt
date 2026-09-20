cmake_minimum_required(VERSION 3.18.1)

project(multiplayermod LANGUAGES C CXX ASM)

if (NOT ANDROID)
    message(FATAL_ERROR "multiplayermod is an Android NDK project. Configure it with android.toolchain.cmake.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    Dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Dobby)

add_library(multiplayermod SHARED src/main.cpp)

target_include_directories(multiplayermod PRIVATE ${dobby_SOURCE_DIR}/include include)

find_library(log-lib log REQUIRED)
find_library(dl-lib dl REQUIRED)

target_link_libraries(multiplayermod PRIVATE ${log-lib} ${dl-lib} dobby)

message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "Android platform: ${ANDROID_PLATFORM}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
