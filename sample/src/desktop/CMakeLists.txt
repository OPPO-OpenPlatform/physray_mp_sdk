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

add_sample_app(cornell ../cornell/main.cpp ../cornell/cornell.h)
add_sample_app(suzanne ../suzanne/main.cpp ../suzanne/suzanne.h)
add_sample_app(shadow ../shadow/main.cpp ../shadow/shadow.h)
add_sample_app(garage ../garage/main.cpp ../garage/garage.h)
add_sample_app(ring ../ring/main.cpp ../ring/ring.h)
add_sample_app(ptdemo ../ptdemo/main.cpp ../ptdemo/ptdemo.h)
