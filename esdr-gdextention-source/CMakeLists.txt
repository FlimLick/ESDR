cmake_minimum_required(VERSION 3.20)
project(esdr_gdextension LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ESDR_ENABLE_NATIVE_OPTIMIZATIONS "Enable -march=native/-mtune=native for local builds." ON)
option(ESDR_ENABLE_LTO "Enable interprocedural optimization (LTO) when supported." ON)
option(ESDR_ENABLE_PKGCONFIG_SOAPY_FALLBACK "Allow pkg-config fallback for SoapySDR discovery (recommended OFF for cross builds)." ON)
option(ESDR_TREAT_WARNINGS_AS_ERRORS "Treat warnings from the ESDR target as build errors." ON)

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
    src/digit_number_selector.cpp
    src/rtl_sdr_source_node.cpp
    src/nfm_demodulator_node.cpp
    src/wfm_demodulator_node.cpp
    src/am_demodulator_node.cpp
    src/ssb_demodulator_node.cpp
    src/cw_demodulator_node.cpp
    src/audio_sink_node.cpp
    src/audio_generator_source_node.cpp
    src/audio_mixer_node.cpp
    src/audio_stereo_matrix_node.cpp
    src/baseband_replay_node.cpp
    src/audio_replay_node.cpp
    src/math_operator_node.cpp
    src/value_type_converter_node.cpp
    src/esdr_graph_factory.cpp
    src/rtl_sdr_backend.cpp
)

target_include_directories(esdr PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(esdr PRIVATE godot-cpp)

# Keep warnings-as-errors focused on ESDR code by silencing diagnostics from
# external godot-cpp headers.
if(TARGET godot-cpp)
    get_target_property(ESDR_GODOT_CPP_INCLUDE_DIRS godot-cpp INTERFACE_INCLUDE_DIRECTORIES)
    if(ESDR_GODOT_CPP_INCLUDE_DIRS)
        target_include_directories(esdr SYSTEM PRIVATE ${ESDR_GODOT_CPP_INCLUDE_DIRS})
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(esdr PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O3>
    )
    if(ESDR_ENABLE_NATIVE_OPTIMIZATIONS AND NOT CMAKE_CROSSCOMPILING)
        target_compile_options(esdr PRIVATE -march=native -mtune=native)
    endif()
endif()

if(ESDR_TREAT_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(esdr PRIVATE /WX)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(esdr PRIVATE -Werror)
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND MINGW)
    # Reduce runtime DLL requirements for distributed Windows builds.
    target_link_options(esdr PRIVATE -static-libstdc++ -static-libgcc)
endif()

if(ESDR_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ESDR_IPO_SUPPORTED)
    if(ESDR_IPO_SUPPORTED)
        set_property(TARGET esdr PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set_property(TARGET esdr PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    endif()
endif()

set(ESDR_SOAPY_PREFIX_HINTS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND ESDR_SOAPY_PREFIX_HINTS
        "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-release"
        "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-debug"
        "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win"
    )
else()
    list(APPEND ESDR_SOAPY_PREFIX_HINTS
        "${CMAKE_CURRENT_SOURCE_DIR}/radio-install"
        "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-linux"
    )
endif()

find_package(SoapySDR CONFIG QUIET
    PATHS ${ESDR_SOAPY_PREFIX_HINTS}
    NO_DEFAULT_PATH
)
if(NOT SoapySDR_FOUND)
    find_package(SoapySDR CONFIG QUIET)
endif()

set(ESDR_HAS_SOAPY FALSE)
if(SoapySDR_FOUND)
    target_compile_definitions(esdr PRIVATE ESDR_HAS_SOAPY=1)
    if(TARGET SoapySDR::SoapySDR)
        target_link_libraries(esdr PRIVATE SoapySDR::SoapySDR)
        set(ESDR_HAS_SOAPY TRUE)
        message(STATUS "Building with SoapySDR support via CMake package target SoapySDR::SoapySDR")
    elseif(TARGET SoapySDR)
        target_link_libraries(esdr PRIVATE SoapySDR)
        set(ESDR_HAS_SOAPY TRUE)
        message(STATUS "Building with SoapySDR support via CMake package target SoapySDR")
    elseif(DEFINED SoapySDR_LIBRARIES)
        target_link_libraries(esdr PRIVATE ${SoapySDR_LIBRARIES})
        if(DEFINED SoapySDR_INCLUDE_DIRS)
            target_include_directories(esdr PRIVATE ${SoapySDR_INCLUDE_DIRS})
        endif()
        set(ESDR_HAS_SOAPY TRUE)
        message(STATUS "Building with SoapySDR support via CMake package variables")
    endif()
elseif(ESDR_ENABLE_PKGCONFIG_SOAPY_FALLBACK AND NOT CMAKE_CROSSCOMPILING)
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
        set(ESDR_HAS_SOAPY TRUE)
        message(STATUS "Building with SoapySDR support via pkg-config")
    endif()
endif()

if(UNIX AND NOT APPLE)
    # Make linux extension resolve bundled .so files from res://bin at runtime.
    set_target_properties(esdr PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
        INSTALL_RPATH_USE_LINK_PATH FALSE
    )

    if(DEFINED SoapySDR_DIR)
        get_filename_component(ESDR_SOAPY_PREFIX "${SoapySDR_DIR}/../../.." ABSOLUTE)
        set(ESDR_SOAPY_LIB_DIR "${ESDR_SOAPY_PREFIX}/lib")
        set(ESDR_SOAPY_MOD_DIR "${ESDR_SOAPY_LIB_DIR}/SoapySDR/modules0.8-3")

        if(EXISTS "${ESDR_SOAPY_LIB_DIR}/libSoapySDR.so.0.8-3")
            add_custom_command(TARGET esdr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ESDR_SOAPY_LIB_DIR}/libSoapySDR.so.0.8-3"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../bin/libSoapySDR.so.0.8-3"
            )
        endif()
        if(EXISTS "${ESDR_SOAPY_LIB_DIR}/librtlsdr.so.0")
            add_custom_command(TARGET esdr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ESDR_SOAPY_LIB_DIR}/librtlsdr.so.0"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../bin/librtlsdr.so.0"
            )
        endif()
        if(EXISTS "${ESDR_SOAPY_LIB_DIR}/libusb-1.0.so.0")
            add_custom_command(TARGET esdr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ESDR_SOAPY_LIB_DIR}/libusb-1.0.so.0"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../bin/libusb-1.0.so.0"
            )
        endif()
        if(EXISTS "${ESDR_SOAPY_MOD_DIR}/librtlsdrSupport.so")
            add_custom_command(TARGET esdr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/../bin/soapy-modules"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ESDR_SOAPY_MOD_DIR}/librtlsdrSupport.so"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../bin/soapy-modules/librtlsdrSupport.so"
            )
        endif()
    endif()
endif()

if(NOT ESDR_HAS_SOAPY)
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

# Match output filenames to esdr.gdextension library mappings so editor/runtime
# always load the freshly built binaries instead of stale leftovers.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(esdr PROPERTIES
        PREFIX ""
        OUTPUT_NAME_DEBUG "esdr.windows.debug"
        OUTPUT_NAME_RELEASE "esdr.windows.release"
        OUTPUT_NAME_RELWITHDEBINFO "esdr.windows.release"
        OUTPUT_NAME_MINSIZEREL "esdr.windows.release"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Ensure runtime DLL dependencies are available for Godot export.
    if(DEFINED SoapySDR_DIR)
        get_filename_component(ESDR_SOAPY_PREFIX "${SoapySDR_DIR}/../../.." ABSOLUTE)
        set(ESDR_SOAPY_RUNTIME_DLL "${ESDR_SOAPY_PREFIX}/bin/libSoapySDR.dll")
        if(EXISTS "${ESDR_SOAPY_RUNTIME_DLL}")
            add_custom_command(TARGET esdr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ESDR_SOAPY_RUNTIME_DLL}"
                    "${ESDR_PROJECT_OUTPUT_DIR}/libSoapySDR.dll"
            )
        endif()

        # Copy Soapy module + RTL runtime DLLs when present.
        set(ESDR_SOAPY_MODULE_DIR_CANDIDATES
            "${ESDR_SOAPY_PREFIX}/lib/SoapySDR/modules0.8"
            "${ESDR_SOAPY_PREFIX}/lib/SoapySDR/modules0.8-3"
            "${ESDR_SOAPY_PREFIX}/lib/SoapySDR/modules0.7"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-release/lib/SoapySDR/modules0.8"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-release/lib/SoapySDR/modules0.8-3"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-debug/lib/SoapySDR/modules0.8"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-debug/lib/SoapySDR/modules0.8-3"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-libs/SoapyRTLSDR/build-win-release"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-libs/SoapyRTLSDR/build-win-debug"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-libs/SoapyRTLSDR/build-win"
        )
        foreach(ESDR_MODULE_DIR IN LISTS ESDR_SOAPY_MODULE_DIR_CANDIDATES)
            if(EXISTS "${ESDR_MODULE_DIR}")
                file(GLOB ESDR_SOAPY_MODULE_DLLS
                    "${ESDR_MODULE_DIR}/*rtlsdr*Support*.dll"
                    "${ESDR_MODULE_DIR}/*RTLSDR*Support*.dll"
                )
                foreach(ESDR_MODULE_DLL IN LISTS ESDR_SOAPY_MODULE_DLLS)
                    get_filename_component(ESDR_MODULE_BASENAME "${ESDR_MODULE_DLL}" NAME)
                    add_custom_command(TARGET esdr POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${ESDR_PROJECT_OUTPUT_DIR}/soapy-modules"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${ESDR_MODULE_DLL}"
                            "${ESDR_PROJECT_OUTPUT_DIR}/soapy-modules/${ESDR_MODULE_BASENAME}"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${ESDR_MODULE_DLL}"
                            "${ESDR_PROJECT_OUTPUT_DIR}/${ESDR_MODULE_BASENAME}"
                    )
                endforeach()
            endif()
        endforeach()

        set(ESDR_SOAPY_RUNTIME_DIR_CANDIDATES
            "${ESDR_SOAPY_PREFIX}/bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-release/bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-install-win-debug/bin"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-libs/rtl-sdr/build-win-release/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-libs/rtl-sdr/build-win-debug/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/radio-libs/rtl-sdr/build-win/src"
        )
        foreach(ESDR_RUNTIME_DIR IN LISTS ESDR_SOAPY_RUNTIME_DIR_CANDIDATES)
            if(EXISTS "${ESDR_RUNTIME_DIR}")
                file(GLOB ESDR_RUNTIME_DLLS
                    "${ESDR_RUNTIME_DIR}/*rtlsdr*.dll"
                    "${ESDR_RUNTIME_DIR}/*RTLSDR*.dll"
                    "${ESDR_RUNTIME_DIR}/libusb-1.0.dll"
                    "${ESDR_RUNTIME_DIR}/libwinpthread-1.dll"
                    "${ESDR_RUNTIME_DIR}/pthreadVC2.dll"
                )
                foreach(ESDR_RUNTIME_DLL IN LISTS ESDR_RUNTIME_DLLS)
                    get_filename_component(ESDR_RUNTIME_BASENAME "${ESDR_RUNTIME_DLL}" NAME)
                    add_custom_command(TARGET esdr POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${ESDR_RUNTIME_DLL}"
                            "${ESDR_PROJECT_OUTPUT_DIR}/${ESDR_RUNTIME_BASENAME}"
                    )
                endforeach()
            endif()
        endforeach()
    endif()

    if(MINGW)
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++-6.dll
            OUTPUT_VARIABLE ESDR_LIBSTDCPP_DLL
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgcc_s_seh-1.dll
            OUTPUT_VARIABLE ESDR_LIBGCC_DLL
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(EXISTS "${ESDR_LIBSTDCPP_DLL}")
            add_custom_command(TARGET esdr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ESDR_LIBSTDCPP_DLL}"
                    "${ESDR_PROJECT_OUTPUT_DIR}/libstdc++-6.dll"
            )
        endif()
        if(EXISTS "${ESDR_LIBGCC_DLL}")
            add_custom_command(TARGET esdr POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ESDR_LIBGCC_DLL}"
                    "${ESDR_PROJECT_OUTPUT_DIR}/libgcc_s_seh-1.dll"
            )
        endif()
    endif()
endif()
