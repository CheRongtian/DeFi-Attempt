cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(DlpRiskEngine LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)
    add_subdirectory(../common ${CMAKE_CURRENT_BINARY_DIR}/common)
    add_subdirectory(../observability ${CMAKE_CURRENT_BINARY_DIR}/observability)
    add_subdirectory(../messaging ${CMAKE_CURRENT_BINARY_DIR}/messaging)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/HomebrewPostgreSQL.cmake)
find_package(libpqxx 8 REQUIRED)

add_library(
    dlp_risk_engine
    STATIC
    src/PostgresRiskRepository.cpp
    src/RiskCalculator.cpp
    src/RiskEngine.cpp
)
add_library(DLP::risk_engine ALIAS dlp_risk_engine)

target_include_directories(dlp_risk_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dlp_risk_engine PUBLIC cxx_std_20)
target_link_libraries(dlp_risk_engine PUBLIC DLP::ethereum_common libpqxx::pqxx)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dlp_risk_engine PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(TARGET DLP::messaging)
    add_library(
        dlp_risk_worker_core
        STATIC
        src/RiskWorker.cpp
    )
    add_library(DLP::risk_worker_core ALIAS dlp_risk_worker_core)
    target_include_directories(dlp_risk_worker_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(dlp_risk_worker_core PUBLIC cxx_std_20)
    target_link_libraries(
        dlp_risk_worker_core
        PUBLIC DLP::risk_engine DLP::messaging libpqxx::pqxx
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_risk_worker_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()

    add_executable(dlp_risk_worker src/WorkerMain.cpp)
    target_link_libraries(dlp_risk_worker PRIVATE DLP::risk_worker_core DLP::observability)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_risk_worker PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)
    set(
        DLP_RISK_ENGINE_TEST_SOURCES
        tests/RiskCalculatorTests.cpp
        tests/RiskEngineTests.cpp
        tests/RiskGoldenTests.cpp
        tests/RiskScaleTests.cpp
    )
    if(TARGET DLP::risk_worker_core)
        list(APPEND DLP_RISK_ENGINE_TEST_SOURCES tests/RiskWorkerTests.cpp)
        set(DLP_RISK_ENGINE_TEST_LIBRARIES DLP::risk_worker_core)
    else()
        set(DLP_RISK_ENGINE_TEST_LIBRARIES DLP::risk_engine)
    endif()
    add_executable(dlp_risk_engine_tests ${DLP_RISK_ENGINE_TEST_SOURCES})
    target_link_libraries(
        dlp_risk_engine_tests
        PRIVATE ${DLP_RISK_ENGINE_TEST_LIBRARIES} GTest::gtest_main
    )
    target_compile_definitions(
        dlp_risk_engine_tests
        PRIVATE DLP_GOLDEN_FILE="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/golden/risk_vectors.json"
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(dlp_risk_engine_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    gtest_discover_tests(dlp_risk_engine_tests DISCOVERY_MODE PRE_TEST)
endif()
