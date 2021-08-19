FROM ubuntu:20.04

# Install dependencies
# DEBIAN_FRONTEND=noninteractive necessary for tzdata dependency package
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
  clang \
  cmake \
  libboost-all-dev \
  libprotobuf-dev \
  protobuf-compiler \
  git \
  pip
RUN update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100 && \
  update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100

# Install SEAL
WORKDIR /home/root
RUN git clone -b v3.6.4 https://github.com/microsoft/SEAL.git && \
  cd SEAL && \
  cmake -DSEAL_THROW_ON_TRANSPARENT_CIPHERTEXT=OFF . && \
  make -j && \
  make install

# Install EVA
WORKDIR /home/root
RUN git clone -b v1.0.1 https://github.com/microsoft/EVA && \
  cd EVA && \
  git submodule update --init && \
  cmake . && \
  make -j && \
  python3 -m pip install -e python && \
  python3 python/setup.py bdist_wheel --dist-dir='.' && \
  python3 -m pip install numpy && \
  python3 tests/all.py
