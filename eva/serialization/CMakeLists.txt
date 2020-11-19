# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS known_type.proto eva.proto ckks.proto seal.proto)
add_library(protobuf OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(protobuf PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(protobuf PUBLIC protobuf::libprotobuf)

target_sources(eva PRIVATE
    $<TARGET_OBJECTS:protobuf>
    known_type.cpp
    save_load.cpp
    eva_serialization.cpp
    ckks_serialization.cpp
    seal_serialization.cpp
)
