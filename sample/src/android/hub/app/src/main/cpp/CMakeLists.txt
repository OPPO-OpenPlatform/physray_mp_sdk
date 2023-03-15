#############################################################################
# Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
# License: No license is required for Oplus internal usage.
#          No external usage is allowed.
#
# File : CMakeLists.txt
#
# Version: 2.0
#
# Date : Feb 2023
#
# Author: Computing & Graphics Research Institute
#
# ------------------ Revision History: ---------------------
#  <version>  <date>  <author>  <desc>
#
###############################################################################

cmake_minimum_required(VERSION 3.16.3) # 3.16.3 is the latest cmake version coming with Ubuntu 20.04

project("hub")

# build physray-sdk
get_filename_component(root-dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../.. ABSOLUTE)
add_subdirectory(${root-dir} ${CMAKE_CURRENT_BINARY_DIR}/native)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library(app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)
message(TARGET_OBJECTS = "${TARGET_OBJECTS}")
# build the native lib
add_library(${PROJECT_NAME} SHARED
        native-lib.cpp
        drag-motion-controller.cpp
        main.cpp
        $<TARGET_OBJECTS:physray-rt-internal> # this is for the ph::rt::unitTest() function
)
target_compile_definitions(${PROJECT_NAME} PRIVATE  SAMPLE_FOLDER="${root-dir}/sample/src")
target_include_directories(${PROJECT_NAME} PRIVATE ${root-dir}/sample/src)
target_link_libraries(${PROJECT_NAME} PRIVATE sample-common app-glue)
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate")
