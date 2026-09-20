cmake_minimum_required(VERSION 3.22)
project(CursedHouseChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ANDROID_ABI)
    set(ANDROID_ABI armeabi-v7a)
endif()

add_library(cursedhouse_chat SHARED
    src/main.cpp
)

target_include_directories(cursedhouse_chat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(cursedhouse_chat PRIVATE
    -O2
    -fno-exceptions
    -fno-rtti
    -ffunction-sections
    -fdata-sections
)

target_link_libraries(cursedhouse_chat
    log
    android
)
