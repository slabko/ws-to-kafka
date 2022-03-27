cmake_minimum_required(VERSION 3.20)

# create `compile_commands.json` for clangd and autocompletion
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall)

project(main)

# Search in our custom directory
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH};thirdparty/bin)

# Add Find*Config.cmake search path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  

set(RDKAFKA_MIN_VERSION "0.9.4")
set(RDKAFKA_MIN_VERSION_HEX 0x00090400)

find_package(Boost REQUIRED iostreams)
find_package(RdKafka REQUIRED)
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS s3)

add_subdirectory(src/wssource)
add_subdirectory(src/s3sink)
