project(s3sink)

add_executable(s3sink main.cpp message_store.cpp)

target_link_libraries(s3sink 
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    RdKafka::rdkafka
    RdKafka::rdkafka++ 
    spdlog::spdlog
    Boost::iostreams
    ${AWSSDK_LINK_LIBRARIES}
)

target_precompile_headers(s3sink PUBLIC ../pch.h)
