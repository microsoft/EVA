#!/bin/sh

set -e

sudo apt update -y
sudo apt install libboost-all-dev libprotobuf-dev protobuf-compiler curl git build-essential cmake automake libtool libtool-bin -y

python -m pip install --upgrade pip
pip install -r requirements_dev.txt

pip install setuptools wheel twine auditwheel
