cmake_minimum_required(VERSION 3.20)

project(DistributedLendingProtocol LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_subdirectory(cpp/common)
add_subdirectory(cpp/indexer)
add_subdirectory(cpp/risk-engine)
add_subdirectory(cpp/tx-manager)
add_subdirectory(cpp/liquidator)
add_subdirectory(cpp/api-server)
