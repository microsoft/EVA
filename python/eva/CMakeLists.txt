# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

pybind11_add_module(eva_py
    wrapper.cpp
)

target_compile_features(eva_py PUBLIC cxx_std_17)
target_link_libraries(eva_py PRIVATE eva)
if (MSVC)
    target_link_libraries(eva_py PUBLIC bcrypt)
endif()
set_target_properties(eva_py PROPERTIES OUTPUT_NAME _eva)
