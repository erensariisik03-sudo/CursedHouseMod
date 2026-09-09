# CMake Minimum Sürümü
cmake_minimum_required(VERSION 3.18.1)

# Proje Adı
project("multiplayermod")

# C++ Standardı (C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android Log kütüphanesini (LOGI, __android_log_print vb. için) sisteme tanımla
find_library(
    log-lib
    log
)

# Ana Kütüphaneyi Tanımla (SHARED = .so çıktısı verir)
add_library(
    multiplayermod
    SHARED
    src/main.cpp
)

# Kütüphaneleri Projeye Bağla (Linker)
target_link_libraries(
    multiplayermod
    ${log-lib}
)
