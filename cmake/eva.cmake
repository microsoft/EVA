# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.
 
set(EVA_SOURCES
    eva/seal/seal.cpp
    eva/util/logging.cpp
    eva/common/reference_executor.cpp
    eva/ckks/ckks_config.cpp

    eva/ir/term.cpp
    eva/ir/program.cpp
    eva/ir/attribute_list.cpp
    eva/ir/attributes.cpp

    eva/eva.cpp
    eva/version.cpp
)

if(USE_GALOIS)
     set(EVA_SOURCES ${EVA_SOURCES}
         eva/util/galois.cpp
     )
endif()

add_library(eva STATIC ${EVA_SOURCES})

add_subdirectory(${PROJECT_SOURCE_DIR}/eva/serialization/)

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
