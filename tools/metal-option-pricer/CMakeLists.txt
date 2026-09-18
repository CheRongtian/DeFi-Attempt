cmake_minimum_required(VERSION 3.20)

project(DlpMetalOptionPricer LANGUAGES CXX OBJCXX)

if(NOT APPLE)
    message(FATAL_ERROR "The Metal option pricer requires macOS")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(nlohmann_json 3.10 REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_program(XCRUN_EXECUTABLE xcrun REQUIRED)

set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/MonteCarlo.metal)
set(SHADER_AIR ${CMAKE_CURRENT_BINARY_DIR}/MonteCarlo.air)
set(SHADER_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/DlpOptionPricer.metallib)

add_custom_command(
    OUTPUT ${SHADER_AIR}
    COMMAND ${XCRUN_EXECUTABLE} -sdk macosx metal -c ${SHADER_SOURCE} -o ${SHADER_AIR}
    DEPENDS ${SHADER_SOURCE}
    VERBATIM
)
add_custom_command(
    OUTPUT ${SHADER_LIBRARY}
    COMMAND ${XCRUN_EXECUTABLE} -sdk macosx metallib ${SHADER_AIR} -o ${SHADER_LIBRARY}
    DEPENDS ${SHADER_AIR}
    VERBATIM
)
add_custom_target(dlp_metal_option_shaders DEPENDS ${SHADER_LIBRARY})

add_executable(
    dlp_metal_option_pricer
    src/Main.cpp
    src/MetalMonteCarlo.mm
    src/OptionPricing.cpp
)
add_dependencies(dlp_metal_option_pricer dlp_metal_option_shaders)
set_source_files_properties(src/MetalMonteCarlo.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_include_directories(dlp_metal_option_pricer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(
    dlp_metal_option_pricer
    PRIVATE DLP_OPTION_METALLIB_PATH="${SHADER_LIBRARY}"
)
target_link_libraries(
    dlp_metal_option_pricer
    PRIVATE
        Boost::system
        nlohmann_json::nlohmann_json
        ${FOUNDATION_FRAMEWORK}
        ${METAL_FRAMEWORK}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(
        dlp_metal_option_pricer
        PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
endif()
