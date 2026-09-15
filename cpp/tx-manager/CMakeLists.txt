cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpTxManager LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
endif()

find_package(libpqxx 8 REQUIRED)
find_package(Threads REQUIRED)

add_library(
    dlp_tx_manager_core
    STATIC
    src/PostgresTransactionStore.cpp
    src/TransactionRpc.cpp
    src/TxManager.cpp
    src/TxTypes.cpp
)
add_library(DLP::tx_manager_core ALIAS dlp_tx_manager_core)
target_include_directories(dlp_tx_manager_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dlp_tx_manager_core PUBLIC cxx_std_20)
target_link_libraries(dlp_tx_manager_core PUBLIC DLP::ethereum_common libpqxx::pqxx)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_tx_manager_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(dlp_tx_manager src/Main.cpp)
target_link_libraries(dlp_tx_manager PRIVATE DLP::tx_manager_core Threads::Threads)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_executable(dlp_tx_manager_tests tests/TxManagerTests.cpp)
    target_link_libraries(dlp_tx_manager_tests PRIVATE DLP::tx_manager_core GTest::gtest_main)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_tx_manager_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    gtest_discover_tests(dlp_tx_manager_tests DISCOVERY_MODE PRE_TEST)
endif()
