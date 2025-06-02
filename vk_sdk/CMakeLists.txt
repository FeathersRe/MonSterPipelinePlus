cmake_minimum_required(VERSION 3.16)
project(vk_sdk VERSION 1.4.0)

#======================================================
# Configuration
#======================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (CapnProto_DIR)
    set(CAPNP_INSTALL_DIR ${CapnProto_DIR}/../../..)
else()
    message(FATAL_ERROR "Failed to find CapnProto installation.")
endif()

if (MSVC)
    set(CMAKE_MSVCIDE_RUN_PATH ${CAPNP_INSTALL_DIR}/bin)
    set(CAPNP_BIN_UNIX_PATH_VAR)
else()
    set(CAPNP_BIN_UNIX_PATH_VAR "PATH=${CAPNP_INSTALL_DIR}/bin:$ENV{PATH}")
endif()

#======================================================
# Dependencies
#======================================================

find_package(eCAL 5.11.8 CONFIG REQUIRED)
find_package(CapnProto 1.0.2 CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)


#=====================================================
# CapNProto Compilation
#======================================================

set(VKC_CAPNP_SCHEMA_FILES
        header.capnp
        vector3.capnp
        imu.capnp
        imulist.capnp
        image.capnp
        odometry3d.capnp
        disparity.capnp
        so3.capnp
        se3.capnp
        detection2d.capnp
        cameracontrol.capnp
        cameraintrinsic.capnp
        sensorextrinsic.capnp
        vector2.capnp
        system.capnp
        tagdetection.capnp
        twist3.capnp
        flow2d.capnp
        pointcloud.capnp
        gps.capnp
)

set(VKC_CAPNP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/vk_sdk/capnp")
set(VKC_CAPNP_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/capnp")

file(MAKE_DIRECTORY ${VKC_CAPNP_INCLUDE_DIR})
file(MAKE_DIRECTORY ${VKC_CAPNP_OUTPUT_DIR})

set(VKC_CAPNP_HEADERS)
set(VKC_CAPNP_SOURCES)

foreach(VKC_CAPNP_SCHEMA_FILE ${VKC_CAPNP_SCHEMA_FILES})
        get_filename_component(VKC_CAPNP_SCHEMA_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/capnp" ABSOLUTE)
        get_filename_component(VKC_CAPNP_SCHEMA_FILE_PATH "${VKC_CAPNP_SCHEMA_FILE_PREFIX}/${VKC_CAPNP_SCHEMA_FILE}" ABSOLUTE)

        if(NOT EXISTS "${VKC_CAPNP_SCHEMA_FILE_PATH}")
                message(FATAL_ERROR "Cap'n Proto schema file '${VKCOMPONENT coreC_CAPNP_SCHEMA_FILE_PATH}' does not exist!")
        endif()

        # https://stackoverflow.com/questions/18590445/cmake-custom-command-to-copy-and-rename

        add_custom_command(
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${VKC_CAPNP_SCHEMA_FILE}.c++" "${CMAKE_CURRENT_BINARY_DIR}/${VKC_CAPNP_SCHEMA_FILE}.h"
                COMMAND ${CAPNP_BIN_UNIX_PATH_VAR} "capnp"
                ARGS compile
                        --output=c++:${CMAKE_CURRENT_BINARY_DIR}
                        --src-prefix=${VKC_CAPNP_SCHEMA_FILE_PREFIX}
                        --import-path=${VKC_CAPNP_SCHEMA_FILE_PREFIX}
                        --import-path=${CAPNP_INCLUDE_DIRECTORY}
                        ${VKC_CAPNP_SCHEMA_FILE_PATH}
                DEPENDS "${VKC_CAPNP_SCHEMA_FILE_PATH}"
                COMMENT "Compiling Cap'n Proto schema ${VKC_CAPNP_SCHEMA_FILE}"
                VERBATIM
        )

        add_custom_command(
            OUTPUT "${VKC_CAPNP_INCLUDE_DIR}/${VKC_CAPNP_SCHEMA_FILE}.h"
            COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE=${CMAKE_CURRENT_BINARY_DIR}/${VKC_CAPNP_SCHEMA_FILE}.h"
                "-DTARGET=${VKC_CAPNP_INCLUDE_DIR}/${VKC_CAPNP_SCHEMA_FILE}.h"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchGeneratedSource.cmake"
            DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${VKC_CAPNP_SCHEMA_FILE}.h"
        )

        add_custom_command(
                OUTPUT "${VKC_CAPNP_OUTPUT_DIR}/${VKC_CAPNP_SCHEMA_FILE}.c++"
                COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${VKC_CAPNP_SCHEMA_FILE}.c++ "${VKC_CAPNP_OUTPUT_DIR}/${VKC_CAPNP_SCHEMA_FILE}.c++"
                DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${VKC_CAPNP_SCHEMA_FILE}.c++"
                VERBATIM
        )

        list(APPEND VKC_CAPNP_HEADERS "${VKC_CAPNP_INCLUDE_DIR}/${VKC_CAPNP_SCHEMA_FILE}.h")
        list(APPEND VKC_CAPNP_SOURCES "${VKC_CAPNP_OUTPUT_DIR}/${VKC_CAPNP_SCHEMA_FILE}.c++")
endforeach()

# also add the shared.hpp
list(APPEND VKC_CAPNP_HEADERS "${VKC_CAPNP_INCLUDE_DIR}/Shared.hpp")

#set(VKC_CAPNP_HEADERS ${VKC_CAPNP_HEADERS} PARENT_SCOPE)


#======================================================
# Targets
#======================================================

add_library(vk_sdk SHARED
    ${VKC_CAPNP_HEADERS}
    ${VKC_CAPNP_SOURCES}

    src/ctrl_c/ctrl-c.h
    src/ctrl_c/ctrl-c.cpp

    include/vk_sdk/Preamble.hpp
    include/vk_sdk/Message.hpp
    include/vk_sdk/Receivers.hpp
    include/vk_sdk/Data.hpp
    include/vk_sdk/VisualKit.hpp
    include/vk_sdk/Logging.hpp
    include/vk_sdk/Utilities.hpp
    include/vk_sdk/DisparityToDepth.hpp
    include/vk_sdk/DisparityToPointCloud.hpp
    include/vk_sdk/DisparityToDistance.hpp
    include/vk_sdk/Sdk.hpp

    src/VisualKit.cpp
    src/Logging.cpp
    src/Utilities.cpp
    src/DisparityToDepth.cpp
    src/DisparityToPointCloud.cpp
    src/DisparityToDistance.cpp
)
target_compile_definitions(vk_sdk PRIVATE VK_SDK_EXPORT)
target_include_directories(vk_sdk 
    PUBLIC  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/vk_sdk/capnp>
            $<INSTALL_INTERFACE:include>
            $<INSTALL_INTERFACE:include>/vk_sdk/capnp
) 
target_link_libraries(vk_sdk
    PUBLIC  CapnProto::capnp eCAL::core 
    PRIVATE ixwebsocket::ixwebsocket
)
set_target_properties(vk_sdk PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(vk_sdk PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION})

#======================================================
# Examples
#======================================================

#if (BUILD_EXAMPLES)
    add_executable(vk_sdk_custom_receiver examples/CustomReceiver.cpp)
    target_link_libraries(vk_sdk_custom_receiver PRIVATE vk_sdk)

    add_executable(vk_sdk_custom_sender examples/CustomSender.cpp)
    target_link_libraries(vk_sdk_custom_sender PRIVATE vk_sdk)

    add_executable(vk_sdk_custom_logger examples/CustomLogger.cpp)
    target_link_libraries(vk_sdk_custom_logger PRIVATE vk_sdk)

    add_executable(vk_sdk_custom_yolo examples/CustomYolo.cpp)
    target_link_libraries(vk_sdk_custom_yolo PRIVATE vk_sdk)
#endif()

#======================================================
# Install
#======================================================

include (GNUInstallDirs)
include (CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file (
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    COMPATIBILITY SameMinorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    COMPONENT core
)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT core
)

install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE vilota::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
        COMPONENT core
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT core)
install(DIRECTORY examples/ DESTINATION examples COMPONENT core)

#install(
#    FILES ${CMAKE_CURRENT_SOURCE_DIR}/../cache/IXWebSocket-install/lib/${CMAKE_STATIC_LIBRARY_PREFIX}ixwebsocket${CMAKE_STATIC_LIBRARY_SUFFIX} 
#    DESTINATION ${CMAKE_INSTALL_LIBDIR})

#install(IMPORTED_RUNTIME_ARTIFACTS ixwebsocket::ixwebsocket 
#    RUNTIME_DEPENDENCY_SET ixwebsocket_deps
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
#install(RUNTIME_DEPENDENCY_SET ixwebsocket_deps LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)

include(CPack)