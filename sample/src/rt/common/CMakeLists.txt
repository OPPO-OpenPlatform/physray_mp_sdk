#############################################################################
# Copyright (C) 2020 - 2023 OPPO. All rights reserved.
###############################################################################


# ======================================================================================================================
#  build sample assets resource library
# ======================================================================================================================

# Compile shaders into SPIRV
set(shaders
    shader/skinned-mesh.comp
    shader/morph-targets.comp
)
PH_compile_glsl_shaders(spirv ${glslc} ${opt} SOURCES ${shaders})

# Generate the resource library
# (No need to include CMakeC.cmake, since it is already included by rt module)
cmrc_add_resource_library(
    sample-asset
    NAMESPACE sampleasset
    WHENCE ${CMAKE_CURRENT_BINARY_DIR}
    ${spirv}
)

# ======================================================================================================================
#  build 3rd party libraries
# ======================================================================================================================

set(IMGUI_SOURCES
    3rdparty/imgui/imgui.cpp
    3rdparty/imgui/imgui_demo.cpp
    3rdparty/imgui/imgui_draw.cpp
    3rdparty/imgui/imgui_tables.cpp
    3rdparty/imgui/imgui_widgets.cpp
    3rdparty/imgui/imgui_impl_vulkan.cpp
)
if (ANDROID)
    list(APPEND IMGUI_SOURCES 3rdparty/imgui/imgui_impl_android.cpp)
else()
    list(APPEND IMGUI_SOURCES 3rdparty/imgui/imgui_impl_glfw.cpp)
endif()
PH_add_library(imgui STATIC ${IMGUI_SOURCES})
#target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_link_libraries(imgui PUBLIC physray-va)
if (NOT ANDROID)
    target_link_libraries(imgui PUBLIC glfw)
endif()

# ======================================================================================================================
#  build sample libraries
# ======================================================================================================================

set(sources
    pch.cpp
    animations/root-transform-channel.cpp
    animations/timeline.cpp
    animations/transform-channel.cpp
    animations/weight-channel.cpp
    first-person-controller.cpp
    gltf/animations/gltf-animation-builder.cpp
    gltf/animations/gltf-timeline-builder.cpp
    gltf/animations/gltf-transform-channel-builder.cpp
    gltf/animations/gltf-weight-channel-builder.cpp
    gltf/gltf-camera-builder.cpp
    gltf/gltf-image-builder.cpp
    gltf/gltf-light-builder.cpp
    gltf/gltf-material-builder.cpp
    gltf/gltf-mesh-builder.cpp
    gltf/gltf-scene-asset-builder.cpp
    gltf/physray-type-converter.cpp
    gltf/gltf.cpp
    gltf-scene-reader.cpp
    image-splicer.cpp
    modelviewer.cpp
    sphere.cpp
    skybox.cpp
    texture-cache.cpp
    ui.cpp
    simpleApp.cpp
    skinning.cpp
    morphtargets.cpp
    fatmesh.cpp
)
PH_setup_pch(${sources} PCH_SOURCE pch.cpp)

# Build a object library that does not link to RT shared library. This is to make it easier for test app to
# choose which RT library, shared or static, to link to later.
PH_add_library(sample-common-objects ${sources})
if(MSVC)
    # enable c++20 standard for MSVC. So we can use the designated initializer feature.
    target_compile_features(sample-common-objects PUBLIC cxx_std_20)
endif()
target_compile_definitions(sample-common-objects PRIVATE ASSET_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../../asset")
target_include_directories(sample-common-objects PUBLIC
    3rdparty/sigslot/include
    $<TARGET_PROPERTY:physray-rt,INCLUDE_DIRECTORIES> # include RT headers, but not link to it.
)
target_link_libraries(sample-common-objects PUBLIC physray-rps physray-va imgui PRIVATE sample-asset)

# Build the official sample-common library
PH_add_library(sample-common $<TARGET_OBJECTS:sample-common-objects>)
target_link_libraries(sample-common PUBLIC sample-common-objects physray-rt)
