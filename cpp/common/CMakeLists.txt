cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpEthereumCommon LANGUAGES C CXX)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
endif()

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

include(FetchContent)

set(ETHASH_BUILD_ETHASH OFF CACHE BOOL "Build only the Ethereum Keccak library" FORCE)
set(ETHASH_INSTALL_CMAKE_CONFIG OFF CACHE BOOL "Skip installing fetched ethash" FORCE)
set(ETHASH_TESTING OFF CACHE BOOL "Skip upstream ethash tests" FORCE)

FetchContent_Declare(
    ethash
    URL https://github.com/chfast/ethash/archive/refs/tags/v1.1.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ethash)

find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(nlohmann_json 3.10 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SECP256K1 REQUIRED IMPORTED_TARGET libsecp256k1)

add_library(
    dlp_ethereum_common
    STATIC
    src/Abi.cpp
    src/Address.cpp
    src/Hex.cpp
    src/Keccak.cpp
    src/ProtocolAbi.cpp
    src/Rlp.cpp
    src/RpcClient.cpp
    src/Transaction.cpp
    src/Uint256.cpp
    src/Uint256Math.cpp
)

add_library(DLP::ethereum_common ALIAS dlp_ethereum_common)

target_include_directories(
    dlp_ethereum_common
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    dlp_ethereum_common
    PUBLIC
        Boost::system
        ethash::keccak
        nlohmann_json::nlohmann_json
        PkgConfig::SECP256K1
)

target_compile_definitions(dlp_ethereum_common PUBLIC BOOST_ASIO_NO_DEPRECATED)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_ethereum_common PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

option(DLP_BUILD_CPP_SMOKE "Build the standalone C++ smoke executable" ON)

if(DLP_BUILD_CPP_SMOKE)
    add_executable(dlp_cpp_smoke smoke/Smoke.cpp)
    target_link_libraries(dlp_cpp_smoke PRIVATE DLP::ethereum_common)

    if(BUILD_TESTING)
        add_test(NAME dlp_cpp_smoke COMMAND dlp_cpp_smoke)
    endif()
endif()

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)

    add_executable(
        dlp_ethereum_tests
        tests/AbiTests.cpp
        tests/AddressTests.cpp
        tests/HexTests.cpp
        tests/KeccakTests.cpp
        tests/ProtocolAbiTests.cpp
        tests/RlpTests.cpp
        tests/RpcClientIntegrationTests.cpp
        tests/TransactionTests.cpp
        tests/Uint256Tests.cpp
        tests/Uint256MathTests.cpp
    )

    target_link_libraries(
        dlp_ethereum_tests
        PRIVATE
            DLP::ethereum_common
            GTest::gtest_main
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_ethereum_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()

    gtest_discover_tests(dlp_ethereum_tests DISCOVERY_MODE PRE_TEST)
endif()
