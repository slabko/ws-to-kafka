#!/bin/sh

# Exit on any non-zero exit
set -e

# git submodule update --init --recursive

cd thirdparty
mkdir bin

# Boost

git clone --depth 1 -b ws-write https://github.com/slabko/ws-to-kafka.git 
wget https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.gz
tar xzf boost_1_78_0.tar.gz
mv boost_1_78_0/* src/boost
rm -r boost_1_78_0
rm boost_1_78_0.tar.gz

cd src/boost && \
./bootstrap.sh --prefix=$(pwd)/../../bin --with-libraries=system,coroutine,regex,iostreams && \
./b2 && \
./b2 install && \
cd ../..

# LibRdKafka

wget https://github.com/edenhill/librdkafka/archive/refs/tags/v1.6.2.tar.gz -O librdkafka.tar.gz
tar xzf librdkafka.tar.gz
mv librdkafka-1.6.2/* src/librdkafka/
rm -r librdkafka-1.6.2
rm librdkafka.tar.gz

cd src/librdkafka && \
./configure --prefix=$(pwd)/../../bin && \
make && \
make install && \
cd ../..

# Aws SDK

wget https://github.com/aws/aws-sdk-cpp/archive/refs/tags/1.9.223.tar.gz -O aws-sdk-cpp.tar.gz
tar xzf aws-sdk-cpp.tar.gz
cd aws-sdk-cpp-1.9.223
./prefetch_crt_dependency.sh
cd ..
mv aws-sdk-cpp-1.9.223/* src/aws-sdk-cpp/
rm -r aws-sdk-cpp-1.9.223
rm aws-sdk-cpp.tar.gz

cd src/aws-sdk-cpp && \
mkdir build && cd build && \
cmake .. \
    -DENABLE_TESTING=OFF \
    -DAUTORUN_UNIT_TESTS=OFF \
    -DBUILD_ONLY="s3" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../bin && \
make -j && \
make install && \
cd ../../..


# spdlog

wget https://github.com/gabime/spdlog/archive/refs/tags/v1.9.2.tar.gz -O spdlog.tar.gz
tar xzf spdlog.tar.gz
mv spdlog-1.9.2/* src/spdlog/
rm -r spdlog-1.9.2
rm spdlog.tar.gz

cd src/spdlog && \
mkdir build && \
cd build && cmake .. -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../bin && \
make -j && \
make install && \
cd ../../..


# Nlohmann JSON
mkdir -p bin/include/nlohmann/
wget https://raw.githubusercontent.com/nlohmann/json/v3.10.5/single_include/nlohmann/json.hpp -O bin/include/nlohmann/json.hpp
