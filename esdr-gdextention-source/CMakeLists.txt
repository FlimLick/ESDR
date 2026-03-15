cmake_minimum_required(VERSION 3.20)
project(esdr_gdextension LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED GODOT_CPP_PATH)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp/CMakeLists.txt")
        set(GODOT_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.godot-cpp/CMakeLists.txt")
        set(GODOT_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.godot-cpp")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../godot-cpp/CMakeLists.txt")
        set(GODOT_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../godot-cpp")
    endif()
endif()

if(NOT DEFINED GODOT_CPP_PATH OR NOT EXISTS "${GODOT_CPP_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "godot-cpp not found. Set -DGODOT_CPP_PATH=/path/to/godot-cpp")
endif()

add_subdirectory("${GODOT_CPP_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp")

add_library(esdr SHARED
    src/register_types.cpp
    src/sdr_node.cpp
    src/rtl_sdr_source_node.cpp
    src/nfm_demodulator_node.cpp
    src/esdr_graph_factory.cpp
    src/rtl_sdr_backend.cpp
)

target_include_directories(esdr PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(esdr PRIVATE godot-cpp)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    set(ESDR_LOCAL_PKGCONFIG "${CMAKE_CURRENT_SOURCE_DIR}/radio-install/lib/pkgconfig")
    if(EXISTS "${ESDR_LOCAL_PKGCONFIG}/SoapySDR.pc")
        if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
            set(ENV{PKG_CONFIG_PATH} "${ESDR_LOCAL_PKGCONFIG}:$ENV{PKG_CONFIG_PATH}")
        else()
            set(ENV{PKG_CONFIG_PATH} "${ESDR_LOCAL_PKGCONFIG}")
        endif()
    endif()
    pkg_check_modules(SOAPYSDR QUIET SoapySDR)
endif()

if(SOAPYSDR_FOUND)
    target_compile_definitions(esdr PRIVATE ESDR_HAS_SOAPY=1)
    target_include_directories(esdr PRIVATE ${SOAPYSDR_INCLUDE_DIRS})
    target_link_libraries(esdr PRIVATE ${SOAPYSDR_LIBRARIES})
    target_link_directories(esdr PRIVATE ${SOAPYSDR_LIBRARY_DIRS})
    message(STATUS "Building with SoapySDR support")
else()
    message(WARNING "SoapySDR not found. RTLSDRBackend will build, but start() will fail until SoapySDR is installed.")
endif()

set(ESDR_PROJECT_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bin")

set_target_properties(esdr PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${ESDR_PROJECT_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${ESDR_PROJECT_OUTPUT_DIR}"
)

foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(esdr PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${cfg} "${ESDR_PROJECT_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_${cfg} "${ESDR_PROJECT_OUTPUT_DIR}"
    )
endforeach()
