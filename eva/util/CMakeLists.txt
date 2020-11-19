# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

if(USE_GALOIS)
    target_sources(eva PRIVATE
        galois.cpp
    )
endif()

target_sources(eva PRIVATE
    logging.cpp
)
