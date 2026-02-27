cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

include(ExternalProject)

project("sidlc" VERSION 0.1.0 LANGUAGES CXX)

# Project configurations
option(BUILD_TESTING "Build tests" OFF)

# Configure header
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE PROJECT_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

# Compiler Tests

# Dependencies
find_package(Python3 COMPONENTS Interpreter)

# Target Configurations
add_executable(sidlc)

# Compile Options
target_compile_features(sidlc PUBLIC cxx_std_20)
target_compile_options(sidlc PUBLIC 
    $<$<COMPILE_LANGUAGE:CXX>:
        -Werror
        -Wall
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -pedantic
        -pedantic-errors
    >
)

target_include_directories(sidlc PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(sidlc PRIVATE "${CMAKE_BINARY_DIR}")

# Subdirectories
add_subdirectory(core)
add_subdirectory(lang)

# Install
install(TARGETS sidlc RUNTIME DESTINATION bin)
install(DIRECTORY interfaces/
    DESTINATION share/sidl/interfaces
    FILES_MATCHING PATTERN "*.sidl"
)
install(FILES cmake/UseSIDLC.cmake
    DESTINATION share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Modules
)

# Additional Targets
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
