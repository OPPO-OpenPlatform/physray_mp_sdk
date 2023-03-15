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

# ======================================================================================================================
# build 3rd party libraries
# ======================================================================================================================

# build glfw
option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
add_subdirectory(3rdparty/glfw)

# ======================================================================================================================
# build sample apps
# ======================================================================================================================
function(add_sample_app name)
    PH_add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE 3rdparty/inc)
    target_link_libraries(${name} PRIVATE sample-common glfw)
    install(TARGETS ${name} RUNTIME DESTINATION sample/bin)
endfunction()

add_sample_app(cornell ../rt/cornell/main.cpp ../rt/cornell/cornell.h)
add_sample_app(suzanne ../rt/suzanne/main.cpp ../rt/suzanne/suzanne.h)
add_sample_app(shadow ../rt/shadow/main.cpp ../rt/shadow/shadow.h)
add_sample_app(garage ../rt/garage/main.cpp ../rt/garage/garage.h)
add_sample_app(war ../rt/war/main.cpp ../rt/war/war.h)
add_sample_app(ring ../rt/ring/main.cpp ../rt/ring/ring.h)
add_sample_app(ptdemo ../rt/ptdemo/main.cpp ../rt/ptdemo/ptdemo.h)
add_sample_app(carousel ../rt/carousel/main.cpp ../rt/carousel/carousel.h)
add_sample_app(empty ../rt/empty_app/main.cpp ../rt/empty_app/empty_app.h)
add_sample_app(triangle ../rt/triangle/main.cpp ../rt/triangle/triangle.h)
add_sample_app(blue_triangle ../rt/blue_triangle/main.cpp ../rt/blue_triangle/blue_triangle.h)

# Copy the physray-rt-shared.dll file to build folder
file(COPY ../../../sdk/win64.Debug/bin/physray-rt-shared.dll DESTINATION Debug)

# RPS samples
add_sample_app(rps-hello ../rps/hello/hello.cpp ../rps/hello/hello.h)
add_sample_app(rps-triangle ../rps/triangle/triangle.cpp ../rps/triangle/triangle.h)
add_sample_app(rps-texture ../rps/texture/texture.cpp ../rps/texture/texture.h)

