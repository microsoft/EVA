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
    VERSION 1.0.1
    LANGUAGES CXX
)

option(USE_GALOIS "Use the Galois library for multicore homomorphic evaluation" OFF)
if(USE_GALOIS)
  message("Galois based multicore support enabled")
  add_definitions(-DEVA_USE_GALOIS)
endif()

find_package(Protobuf 3.6 REQUIRED)

include(cmake/seal.cmake)
include(cmake/pybind11.cmake)
if(USE_GALOIS)
    include(cmake/galois.cmake)
endif()

include(cmake/eva.cmake)
include(cmake/python.cmake)
