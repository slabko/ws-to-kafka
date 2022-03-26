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


from gcc
run apt update && apt install libsasl2-dev libzstd-dev liblz4-dev && apt clean
copy --from=0 /root/ws-to-kafka/build/src/wssource/wssource /usr/bin/
copy --from=0 /root/ws-to-kafka/build/src/s3sink/s3sink /usr/bin/
copy --from=0 /root/ws-to-kafka/thirdparty/bin/lib/librdkafka.so.1 /usr/lib/x86_64-linux-gnu/
copy --from=0 /root/ws-to-kafka/thirdparty/bin/lib/librdkafka++.so.1 /usr/lib/x86_64-linux-gnu/
copy --from=0 /root/ws-to-kafka/thirdparty/bin/lib/libaws-cpp-sdk-s3.so /usr/lib/x86_64-linux-gnu/
copy --from=0 /root/ws-to-kafka/thirdparty/bin/lib/libaws-cpp-sdk-core.so /usr/lib/x86_64-linux-gnu/
