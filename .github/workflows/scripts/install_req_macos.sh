#!/bin/sh

set -e

python -m pip install --upgrade pip
pip install -r examples/requirements.txt

pip install setuptools wheel twine auditwheel
