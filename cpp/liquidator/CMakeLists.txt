cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpLiquidator LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
    add_subdirectory(../messaging ${CMAKE_CURRENT_BINARY_DIR}/messaging)
    add_subdirectory(../risk-engine ${CMAKE_CURRENT_BINARY_DIR}/risk-engine)
    add_subdirectory(../tx-manager ${CMAKE_CURRENT_BINARY_DIR}/tx-manager)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/HomebrewPostgreSQL.cmake)
find_package(libpqxx 8 REQUIRED)
find_package(Threads REQUIRED)

add_library(
    dlp_liquidator_core
    STATIC
    src/LiquidationChain.cpp
    src/LiquidationJobs.cpp
    src/Liquidator.cpp
)
add_library(DLP::liquidator_core ALIAS dlp_liquidator_core)
target_include_directories(dlp_liquidator_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dlp_liquidator_core PUBLIC cxx_std_20)
target_link_libraries(
    dlp_liquidator_core
    PUBLIC
        DLP::ethereum_common
        DLP::messaging
        DLP::risk_engine
        DLP::tx_manager_core
        libpqxx::pqxx
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_liquidator_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(dlp_liquidator src/Main.cpp)
target_link_libraries(dlp_liquidator PRIVATE DLP::liquidator_core Threads::Threads)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_executable(
        dlp_liquidator_tests
        tests/LiquidationJobStoreTests.cpp
        tests/LiquidatorTests.cpp
    )
    target_link_libraries(
        dlp_liquidator_tests
        PRIVATE DLP::liquidator_core GTest::gtest_main Threads::Threads
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_liquidator_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    gtest_discover_tests(dlp_liquidator_tests DISCOVERY_MODE PRE_TEST)
endif()
