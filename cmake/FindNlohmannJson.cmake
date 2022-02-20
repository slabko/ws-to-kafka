
find_path(NlohmannJson_INCLUDE_DIR
    NAMES nlohmann/json.hpp
)

# Check lib paths
if (NLOHMANNJSON_CMAKE_VERBOSE)
    message(STATUS "NlohmannJson_INCLUDE_DIR = ${NlohmannJson_INCLUDE_DIR}")
endif()

# include(FindPackageHandleStandardArgs)
# find_package_handle_standard_args(NlohmannJson DEFAULT_MSG
#     NlohmannJson_INCLUDE_DIR
# )

if(NlohmannJson_INCLUDE_DIR)
    set(NlohmannJson_FOUND TRUE)
    add_library(NlohmannJson INTERFACE)
    set_target_properties(NlohmannJson PROPERTIES
            IMPORTED_NAME NlohmannJson
            INTERFACE_INCLUDE_DIRECTORIES "${NlohmannJson_INCLUDE_DIR}")

    message(STATUS "Found nlohmann/json")
    mark_as_advanced(NlohmannJson_INCLUDE_DIR)
else()
    message(FATAL_ERROR "Failed to find nlohmann/json")
endif()
