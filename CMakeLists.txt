# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0079 NEW)
cmake_policy(SET CMP0076 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(eva
    VERSION 1.0.0
    LANGUAGES CXX
)

option(USE_GALOIS "Use the Galois library for multicore homomorphic evaluation" OFF)
if(USE_GALOIS)
  message("Galois based multicore support enabled")
  add_definitions(-DEVA_USE_GALOIS)
endif()

find_package(SEAL 3.6 REQUIRED)
find_package(Protobuf 3.6 REQUIRED)
find_package(Python COMPONENTS Interpreter Development)

if(NOT Python_VERSION_MAJOR EQUAL 3)
    message(FATAL_ERROR "EVA requires Python 3. Please ensure you have it
        installed in a location searched by CMake.")
endif()

add_subdirectory(third_party/pybind11)
if(USE_GALOIS)
    add_subdirectory(third_party/Galois EXCLUDE_FROM_ALL)
endif()

add_subdirectory(eva)
add_subdirectory(python)
