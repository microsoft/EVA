#!/bin/bash

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

BASE_DIR=$(dirname "$0")
PROJECT_ROOT_DIR=$BASE_DIR/../
shopt -s globstar
clang-format -i $PROJECT_ROOT_DIR/eva/**/*.h
clang-format -i $PROJECT_ROOT_DIR/eva/**/*.cpp
