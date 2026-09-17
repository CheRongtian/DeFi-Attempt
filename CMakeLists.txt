cmake_minimum_required(VERSION 3.20)

project(DistributedLendingProtocol LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(cmake/HomebrewPostgreSQL.cmake)

add_subdirectory(cpp/common)
add_subdirectory(cpp/observability)
add_subdirectory(cpp/messaging)
add_subdirectory(cpp/indexer)
add_subdirectory(cpp/risk-engine)
add_subdirectory(cpp/tx-manager)
add_subdirectory(cpp/liquidator)
add_subdirectory(cpp/api-server)
