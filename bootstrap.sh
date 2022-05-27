#!/bin/sh

# Exit on any non-zero exit
set -e

# git submodule update --init --recursive

cd thirdparty
mkdir bin

# Boost

wget -q https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.gz
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

wget -q https://github.com/edenhill/librdkafka/archive/refs/tags/v1.6.2.tar.gz -O librdkafka.tar.gz
tar xzf librdkafka.tar.gz
mv librdkafka-1.6.2/* src/librdkafka/
rm -r librdkafka-1.6.2
rm librdkafka.tar.gz

cd src/librdkafka && \
./configure --prefix=$(pwd)/../../bin && \
make && \
make install && \
cd ../..

# spdlog

wget -q https://github.com/gabime/spdlog/archive/refs/tags/v1.9.2.tar.gz -O spdlog.tar.gz
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
wget -q https://raw.githubusercontent.com/nlohmann/json/v3.10.5/single_include/nlohmann/json.hpp -O bin/include/nlohmann/json.hpp
