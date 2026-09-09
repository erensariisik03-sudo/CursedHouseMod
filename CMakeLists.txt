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
FetchContent_MakeAvailable(dobby)

# DİKKAT: GITHUB ACTIONS / LİNUX DÜZELTMESİ
# Dobby'nin kendi hedefine kaynak dizinlerini (source) ZORLA tanıtıyoruz
# Böylece "Cpu.h file not found" hatasını aşmış oluyoruz.
target_include_directories(dobby PUBLIC
    ${dobby_SOURCE_DIR}/source
    ${dobby_SOURCE_DIR}/include
)

# Android Log kütüphanesini sisteme tanımla
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

# Dobby Başlık Yollarını Kendi Ana Projemize Dahil Et
target_include_directories(multiplayermod PRIVATE ${dobby_SOURCE_DIR}/include)

# Kütüphaneleri Projeye Bağla (Linker)
target_link_libraries(
    multiplayermod
    
    # Android Log kütüphanesi
    ${log-lib}
    
    # Dobby Hook kütüphanesi
    dobby
)
