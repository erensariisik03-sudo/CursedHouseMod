# CMake Minimum Sürümü
cmake_minimum_required(VERSION 3.18.1)

# Proje Adı
project("multiplayermod")

# C++ Standardı (C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dobby Hook Kütüphanesini GitHub'dan Otomatik İndir ve Hazırla
include(FetchContent)
FetchContent_Declare(
    dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG main
)
FetchContent_MakeAvailable(dobby)

# Android Log kütüphanesini (LOGI, __android_log_print vb. için) sisteme tanımla
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

# Dobby Başlık (Header) Yollarını Projeye Dahil Et
target_include_directories(
    multiplayermod PRIVATE
    ${dobby_SOURCE_DIR}/include
)

# Kütüphaneleri Projeye Bağla (Linker)
target_link_libraries(
    multiplayermod
    
    # Android Log kütüphanesi
    ${log-lib}
    
    # Dobby Hook Kütüphanesi
    dobby
)
