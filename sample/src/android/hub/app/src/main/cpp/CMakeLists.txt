
cmake_minimum_required(VERSION 3.16.3) # 3.16.3 is the latest cmake version coming with Ubuntu 20.04

project("hub")

# include standalone sdk build script, if exists
set(SDK_PACKAGING_CMAKE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/standalone.cmake)
if (EXISTS ${SDK_PACKAGING_CMAKE_FILE})
    # build standalone sample folder
    include(${SDK_PACKAGING_CMAKE_FILE})
    get_filename_component(root-dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../.. ABSOLUTE)
    add_subdirectory(${root-dir} ${CMAKE_CURRENT_BINARY_DIR}/native)
    set(SAMPLE_FOLDER ${root-dir}/sample/src)
else()
    # build the full sdk
    get_filename_component(root-dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../.. ABSOLUTE)
    add_subdirectory(${root-dir} ${CMAKE_CURRENT_BINARY_DIR}/native)
    set(SAMPLE_FOLDER ${root-dir}/sample)
endif()

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library(app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

# build the native lib
add_library(${PROJECT_NAME} SHARED
    native-lib.cpp
    drag-motion-controller.cpp
    main.cpp
)
target_compile_definitions(${PROJECT_NAME} PRIVATE SAMPLE_FOLDER=${SAMPLE_FOLDER})
target_include_directories(${PROJECT_NAME} PRIVATE ${SAMPLE_FOLDER})
target_link_libraries(${PROJECT_NAME} PRIVATE sample-common app-glue)
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate")
