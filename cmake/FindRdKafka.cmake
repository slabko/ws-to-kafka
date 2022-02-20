# Modified version for C++ from CppKafka https://github.com/mfontanini/cppkafka
#
# Original version can be found at
# https://github.com/mfontanini/cppkafka/blob/master/cmake/FindRdKafka.cmake
#
# This find module helps find the RdKafka module. It exports the following variables:
# - RdKafka_INCLUDE_DIR : The directory where rdkafka.h is located.
# - RdKafka_LIBNAME : The name of the library, i.e. librdkafka.a, librdkafka.so, etc.
# - RdKafka_LIBRARY_PATH : The full library path i.e. <path_to_binaries>/${RdKafka_LIBNAME}
# - RdKafka::rdkafka : Imported library containing all above properties set.

if (CPPKAFKA_RDKAFKA_STATIC_LIB)
    set(RDKAFKA_PREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
    set(RDKAFKA_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})
    set(RDKAFKA_LIBRARY_TYPE STATIC)
else()
    set(RDKAFKA_PREFIX ${CMAKE_SHARED_LIBRARY_PREFIX})
    set(RDKAFKA_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(RDKAFKA_LIBRARY_TYPE SHARED)
endif()

set(RdKafkaCpp_LIBNAME ${RDKAFKA_PREFIX}rdkafka++${RDKAFKA_SUFFIX})
set(RdKafkaC_LIBNAME ${RDKAFKA_PREFIX}rdkafka${RDKAFKA_SUFFIX})

find_path(RdKafka_INCLUDE_DIR
    NAMES librdkafka/rdkafkacpp.h
    HINTS ${RdKafka_ROOT}/include
)

find_library(RdKafkaC_LIBRARY_PATH
    NAMES ${RdKafkaC_LIBNAME}
    HINTS ${RdKafka_ROOT}/lib ${RdKafka_ROOT}/lib64
)

find_library(RdKafkaCpp_LIBRARY_PATH
    NAMES ${RdKafkaCpp_LIBNAME}
    HINTS ${RdKafka_ROOT}/lib ${RdKafka_ROOT}/lib64
)

# Check lib paths
if (CPPKAFKA_CMAKE_VERBOSE)
    get_property(FIND_LIBRARY_32 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB32_PATHS)
    get_property(FIND_LIBRARY_64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
    message(STATUS "RDKAFKA search 32-bit library paths: ${FIND_LIBRARY_32}")
    message(STATUS "RDKAFKA search 64-bit library paths: ${FIND_LIBRARY_64}")
    message(STATUS "RdKafka_ROOT = ${RdKafka_ROOT}")
    message(STATUS "RdKafka_INCLUDE_DIR = ${RdKafka_INCLUDE_DIR}")
    message(STATUS "RdKafkaC_LIBNAME = ${RdKafkaC_LIBNAME}")
    message(STATUS "RdKafkaCpp_LIBNAME = ${RdKafkaCpp_LIBNAME}")
    message(STATUS "RdKafkaC_LIBRARY_PATH = ${RdKafkaC_LIBRARY_PATH}")
    message(STATUS "RdKafkaCpp_LIBRARY_PATH = ${RdKafkaCpp_LIBRARY_PATH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RdKafka DEFAULT_MSG
    RdKafkaC_LIBNAME
    RdKafkaCpp_LIBNAME
    RdKafkaC_LIBRARY_PATH
    RdKafkaCpp_LIBRARY_PATH
    RdKafka_INCLUDE_DIR
)

set(CONTENTS "#include <librdkafka/rdkafka.h>\n #if RD_KAFKA_VERSION >= ${RDKAFKA_MIN_VERSION_HEX}\n int main() { }\n #endif")
set(FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/rdkafka_version_test.cpp)
file(WRITE ${FILE_NAME} ${CONTENTS})

try_compile(RdKafka_FOUND ${CMAKE_CURRENT_BINARY_DIR}
            SOURCES ${FILE_NAME}
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${RdKafka_INCLUDE_DIR}")

if (RdKafka_FOUND)
    if (UNIX AND NOT APPLE)
        set(RDKAFKA_DEPENDENCIES pthread rt ssl crypto dl z sasl2 zstd lz4)
    else()
        # That part in not implemented, need to check it on win and mac
        set(RDKAFKA_DEPENDENCIES pthread ssl crypto dl z)
    endif()

    add_library(RdKafka::rdkafka ${RDKAFKA_LIBRARY_TYPE} IMPORTED GLOBAL)
    set_target_properties(RdKafka::rdkafka PROPERTIES
            IMPORTED_NAME RdKafka
            IMPORTED_LOCATION "${RdKafkaC_LIBRARY_PATH}"
            INTERFACE_INCLUDE_DIRECTORIES "${RdKafka_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${RDKAFKA_DEPENDENCIES}")

    add_library(RdKafka::rdkafka++ ${RDKAFKA_LIBRARY_TYPE} IMPORTED GLOBAL)
    set_target_properties(RdKafka::rdkafka++ PROPERTIES
            IMPORTED_NAME RdKafka
            IMPORTED_LOCATION "${RdKafkaCpp_LIBRARY_PATH}"
            INTERFACE_INCLUDE_DIRECTORIES "${RdKafka_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${RDKAFKA_DEPENDENCIES}")

    message(STATUS "Found valid rdkafka version")

    mark_as_advanced(
        RDKAFKAC_LIBRARY
        RDKAFKACpp_LIBRARY
        RdKafka_INCLUDE_DIR
        RdKafkaC_LIBRARY_PATH
        RdKafkaCpp_LIBRARY_PATH
    )
else()
    message(FATAL_ERROR "Failed to find valid rdkafka version")
endif()
