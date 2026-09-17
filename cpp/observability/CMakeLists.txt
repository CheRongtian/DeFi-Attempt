cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpObservability LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
endif()

include(FetchContent)

set(ENABLE_PULL OFF CACHE BOOL "" FORCE)
set(ENABLE_PUSH OFF CACHE BOOL "" FORCE)
set(ENABLE_COMPRESSION OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(GENERATE_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_STANDARD_FLAGS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    prometheus_cpp
    URL https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.3.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(prometheus_cpp)

find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

add_library(
    dlp_observability
    STATIC
    src/Metrics.cpp
    src/RpcMetrics.cpp
)
add_library(DLP::observability ALIAS dlp_observability)

target_include_directories(
    dlp_observability
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(dlp_observability PUBLIC cxx_std_20)
target_link_libraries(
    dlp_observability
    PUBLIC
        DLP::ethereum_common
        prometheus-cpp::core
    PRIVATE
        Boost::system
        Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_observability PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
