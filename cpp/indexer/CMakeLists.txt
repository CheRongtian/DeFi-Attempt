cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpIndexer LANGUAGES C CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
endif()

if(APPLE AND NOT PostgreSQL_ROOT)
    find_program(DLP_HOMEBREW_EXECUTABLE brew)
    if(DLP_HOMEBREW_EXECUTABLE)
        execute_process(
            COMMAND ${DLP_HOMEBREW_EXECUTABLE} --prefix libpq
            RESULT_VARIABLE DLP_HOMEBREW_LIBPQ_RESULT
            OUTPUT_VARIABLE DLP_HOMEBREW_LIBPQ_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(DLP_HOMEBREW_LIBPQ_RESULT EQUAL 0)
            set(PostgreSQL_ROOT "${DLP_HOMEBREW_LIBPQ_PREFIX}")
            list(PREPEND CMAKE_PREFIX_PATH "${DLP_HOMEBREW_LIBPQ_PREFIX}")
        endif()
    endif()
endif()

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
target_link_libraries(dlp_indexer PRIVATE DLP::indexer_core Threads::Threads)

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
