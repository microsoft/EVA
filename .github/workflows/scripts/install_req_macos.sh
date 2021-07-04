#!/bin/sh

set -e

python -m pip install --upgrade pip
pip install -r requirements_dev.txt
brew install protobuf

pip install setuptools wheel twine auditwheel
