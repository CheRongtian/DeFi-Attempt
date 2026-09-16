cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpMessaging LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
endif()

include(FetchContent)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/HomebrewPostgreSQL.cmake)
find_package(libpqxx 8 REQUIRED)
find_package(nlohmann_json 3.10 REQUIRED)

set(NATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_LIB_SHARED OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_LIB_STATIC ON CACHE BOOL "" FORCE)
set(NATS_BUILD_STREAMING OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_WITH_TLS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    nats_c
    URL https://github.com/nats-io/nats.c/archive/refs/tags/v3.11.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(DLP_BUILD_TESTING ${BUILD_TESTING})
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(nats_c)
set(BUILD_TESTING ${DLP_BUILD_TESTING})

add_library(
    dlp_messaging
    STATIC
    src/Event.cpp
    src/JetStream.cpp
    src/Outbox.cpp
)
add_library(DLP::messaging ALIAS dlp_messaging)

target_include_directories(
    dlp_messaging
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${nats_c_SOURCE_DIR}/src
)
target_compile_features(dlp_messaging PUBLIC cxx_std_20)
target_link_libraries(
    dlp_messaging
    PUBLIC DLP::ethereum_common libpqxx::pqxx nlohmann_json::nlohmann_json
    PRIVATE nats_static
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_messaging PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(dlp_outbox_publisher src/OutboxMain.cpp)
target_link_libraries(dlp_outbox_publisher PRIVATE DLP::messaging)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_outbox_publisher PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_executable(dlp_messaging_tests tests/MessagingTests.cpp)
    target_link_libraries(dlp_messaging_tests PRIVATE DLP::messaging GTest::gtest_main)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_messaging_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    gtest_discover_tests(dlp_messaging_tests DISCOVERY_MODE PRE_TEST)
endif()
