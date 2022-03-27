project(wssource)

add_executable(wssource main.cpp kafka_producer.cpp wsclient.cpp)

target_include_directories(wssource PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(wssource 
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    RdKafka::rdkafka
    RdKafka::rdkafka++ 
    spdlog::spdlog
)

target_precompile_headers(wssource PUBLIC ../pch.h)
