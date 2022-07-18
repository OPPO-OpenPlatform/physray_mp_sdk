project(sample-common)

# ======================================================================================================================
# Build imgui
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
target_link_libraries(imgui PUBLIC physray-va)
if (NOT ANDROID)
    target_link_libraries(imgui PUBLIC glfw)
endif()

# ======================================================================================================================
# Build sample-common
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
    gltf/gltf-scene-builder.cpp
    gltf/physray-type-converter.cpp
    gltf/gltf.cpp
    gltf-scene-reader.cpp
    image-splicer.cpp
    modelviewer.cpp
    sphere.cpp
    skybox.cpp
    texture-cache.cpp
    ui.cpp
    vkutils.cpp
    skinning.cpp
)

PH_setup_pch(${sources} PCH_SOURCE pch.cpp)
PH_add_library(sample-common STATIC ${sources})
if (MSVC)
    # enable c++20 standard for MSVC. So we can use the designated initializer feature.
    target_compile_features(sample-common PUBLIC cxx_std_20)
endif()

target_compile_definitions(sample-common PRIVATE ASSET_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../asset")
target_include_directories(sample-common PUBLIC 3rdparty/sigslot/include)

# setup dependencies
target_link_libraries(sample-common PUBLIC physray-rt physray-va imgui)
