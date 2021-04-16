#!/bin/sh

cmake . && make -j
python -m pip install -e python/
