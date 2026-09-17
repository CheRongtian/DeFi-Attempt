cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpIndexer LANGUAGES C CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
    add_subdirectory(../observability ${CMAKE_CURRENT_BINARY_DIR}/observability)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/HomebrewPostgreSQL.cmake)
find_package(libpqxx 8 REQUIRED)
find_package(Threads REQUIRED)

add_library(
    dlp_indexer_core
    STATIC
    src/EventDecoder.cpp
    src/Indexer.cpp
    src/PostgresStore.cpp
    src/RpcChainClient.cpp
    src/StateProjector.cpp
)

add_library(DLP::indexer_core ALIAS dlp_indexer_core)

target_include_directories(
    dlp_indexer_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(dlp_indexer_core PUBLIC cxx_std_20)

target_link_libraries(
    dlp_indexer_core
    PUBLIC
        DLP::ethereum_common
        libpqxx::pqxx
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_indexer_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(dlp_indexer src/Main.cpp)
target_link_libraries(dlp_indexer PRIVATE DLP::indexer_core DLP::observability Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_indexer PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)

    add_executable(
        dlp_indexer_tests
        tests/EventDecoderTests.cpp
        tests/IndexerTests.cpp
        tests/PostgresStoreIntegrationTests.cpp
        tests/RpcChainClientIntegrationTests.cpp
        tests/StateProjectorTests.cpp
    )

    target_link_libraries(
        dlp_indexer_tests
        PRIVATE
            DLP::indexer_core
            GTest::gtest_main
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_indexer_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()

    gtest_discover_tests(dlp_indexer_tests DISCOVERY_MODE PRE_TEST)
endif()
