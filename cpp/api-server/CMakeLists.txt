cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpApiServer LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
    add_subdirectory(../observability ${CMAKE_CURRENT_BINARY_DIR}/observability)
    add_subdirectory(../risk-engine ${CMAKE_CURRENT_BINARY_DIR}/risk-engine)
endif()

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/HomebrewPostgreSQL.cmake)
find_package(libpqxx 8 REQUIRED)

add_library(
    dlp_api_server_core
    STATIC
    src/ApiChain.cpp
    src/ApiService.cpp
    src/ApiStore.cpp
    src/HttpServer.cpp
)
add_library(DLP::api_server_core ALIAS dlp_api_server_core)
target_include_directories(dlp_api_server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dlp_api_server_core PUBLIC cxx_std_20)
target_link_libraries(
    dlp_api_server_core
    PUBLIC DLP::ethereum_common DLP::risk_engine Boost::system libpqxx::pqxx Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_api_server_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(dlp_api_server src/Main.cpp)
target_link_libraries(dlp_api_server PRIVATE DLP::api_server_core DLP::observability)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)
    add_executable(dlp_api_server_tests tests/ApiServiceTests.cpp)
    target_link_libraries(dlp_api_server_tests PRIVATE DLP::api_server_core GTest::gtest_main)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_api_server_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    gtest_discover_tests(dlp_api_server_tests DISCOVERY_MODE PRE_TEST)
endif()
