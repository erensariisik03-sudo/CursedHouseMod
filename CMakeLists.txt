# CMake Minimum Sürümü
cmake_minimum_required(VERSION 3.18.1)

# Proje Adı
project("multiplayermod")

# C++ Standardı (C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dobby Hook Kütüphanesi
include(FetchContent)
FetchContent_Declare(
    dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG master
)

# Dobby indirildikten sonra kendi include yollarını globale ekliyoruz
FetchContent_GetProperties(dobby)
if(NOT dobby_POPULATED)
    FetchContent_Populate(dobby)
    include_directories(
        ${dobby_SOURCE_DIR}
        ${dobby_SOURCE_DIR}/include
        ${dobby_SOURCE_DIR}/source
    )
    add_subdirectory(${dobby_SOURCE_DIR} ${dobby_BINARY_DIR})
endif()

# Android Log kütüphanesini sisteme tanımla
find_library(
    log-lib
    log
)

# Ana Kütüphaneyi Tanımla (SHARED = .so çıktısı verir)
add_library(
    multiplayermod # Çıktı adı: libmultiplayermod.so olacaktır
    SHARED
    
    # Derlenecek C++ Kaynak Dosyaları
    src/main.cpp
)

# Kütüphaneleri Projeye Bağla (Linker)
target_link_libraries(
    multiplayermod
    
    # Android Log kütüphanesi
    ${log-lib}
    
    # Dobby Hook kütüphanesi
    dobby
)
