# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

add_library(eva STATIC
    eva.cpp
    version.cpp
)

# TODO: everything except SEAL::seal should be make PRIVATE
target_link_libraries(eva PUBLIC SEAL::seal protobuf::libprotobuf)
if(USE_GALOIS)
    target_link_libraries(eva PUBLIC Galois::shmem numa)
endif()
target_include_directories(eva
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)
target_compile_definitions(eva PRIVATE EVA_VERSION_STR="${PROJECT_VERSION}")

add_subdirectory(util)
add_subdirectory(serialization)
add_subdirectory(ir)
add_subdirectory(common)
add_subdirectory(ckks)
add_subdirectory(seal)
