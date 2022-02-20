Prepare the dependencies

```
git submodule update --init --recursive
mkdir thirdparty/bin

cd thirdparty/src/boost && ./bootstrap.sh --prefix=$(pwd)/../../bin && ./b2 &&  ./b2 install && cd ../../..
cd thirdparty/src/librdkafka && ./configure --prefix=$(pwd)/../../bin && make && make install && cd ../../..
cd thirdparty/src/spdlog && mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../bin && make -j && make install && cd ../../../..
mkdir -p thirdparty/bin/include/nlohmann/ && cp thirdparty/src/json/single_include/nlohmann/json.hpp thirdparty/bin/include/nlohmann/
```
