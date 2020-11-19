# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

configure_file(setup.py.in setup.py.after_configure)
file(GENERATE OUTPUT setup.py INPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py.after_configure)

add_subdirectory(eva)

add_custom_target(python ALL DEPENDS eva_py)
add_custom_command(TARGET python
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/eva
            ${CMAKE_CURRENT_BINARY_DIR}/eva
)
