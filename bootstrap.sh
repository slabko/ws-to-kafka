#!/bin/sh

# Exit on any non-zero exit
set -e

git submodule update --init --recursive

mkdir thirdparty/bin

cd thirdparty/src/boost && \
./bootstrap.sh --prefix=$(pwd)/../../bin --with-libraries=system,coroutine,regex,iostreams && \
./b2 && \
./b2 install && \
cd ../../..

cd thirdparty/src/librdkafka && \
./configure --prefix=$(pwd)/../../bin && \
make && \
make install && \
cd ../../..

cd thirdparty/src/spdlog && \
mkdir build && \
cd build && cmake .. -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../bin && \
make -j && \
make install && \
cd ../../../..

mkdir -p thirdparty/bin/include/nlohmann/ && \
cp thirdparty/src/json/single_include/nlohmann/json.hpp thirdparty/bin/include/nlohmann/

cd thirdparty/src/aws-sdk-cpp && \
mkdir build && cd build && \
cmake .. \
    -DBUILD_ONLY="s3" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../bin && \
make -j && \
make install && \
cd ../../../..
