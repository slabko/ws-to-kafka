from slabko/gcc-cmake

run mkdir /root/ws-to-kafka
workdir /root/ws-to-kafka

copy src ./src
copy cmake ./cmake
copy thirdparty ./thirdparty
copy bootstrap.sh ./bootstrap.sh
copy CMakeLists.txt ./CMakeLists.txt

run apt update && apt install libsasl2-dev libzstd-dev liblz4-dev
run sh bootstrap.sh

run mkdir build
workdir build
run cmake .. && make -j


from gcc:11
copy --from=0 /root/ws-to-kafka/build/wssource /usr/bin/
copy --from=0 /root/ws-to-kafka/thirdparty/bin/lib/librdkafka.so.1 /usr/lib/x86_64-linux-gnu/
copy --from=0 /root/ws-to-kafka/thirdparty/bin/lib/librdkafka++.so.1 /usr/lib/x86_64-linux-gnu/
