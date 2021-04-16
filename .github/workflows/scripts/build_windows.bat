@echo off

set PATH=%PATH%;%programfiles%/protobuf/bin/;%programfiles(x86)%/protobuf/bin/

python -m pip install -e python/
