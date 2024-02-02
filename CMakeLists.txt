cmake_minimum_required(VERSION 3.16.3) # 3.16.3 is the latest cmake version coming with Ubuntu 20.04
project(physray-sdk-samples)

# ======================================================================================================================
# Define common build options and flags
# ======================================================================================================================
macro(PH_print_status)
    message("[PhysRay-CMAKE] ${ARGN}")
endmacro()

macro(PH_warning)
    message(WARNING "========================\n\n[PhysRay-CMAKE] ${ARGN}\n\n========================")
endmacro()

# ======================================================================================================================
# utility functions
# ======================================================================================================================

# Setup PCH for source files
#
# Usage:
# PH_setup_pch(source1 [source2 ...] PCH_SOURCE pch_source [PCH_HEADER pch_header])
#
# Default pch header is pch.h, if not specified.
# ------------------------------------------------------------------------------
function(PH_setup_pch)
    string(FIND ${CMAKE_MAKE_PROGRAM} MSBuild isMsBuild)

    if(${isMsBuild} GREATER 0)
        set(oneValueArgs PCH_SOURCE PCH_HEADER)
        cmake_parse_arguments(arg "" "${oneValueArgs}" "" ${ARGN})

        if(arg_UNPARSED_ARGUMENTS AND arg_PCH_SOURCE)
            if(NOT arg_PCH_HEADER)
                set(arg_PCH_HEADER pch.h)
            endif()

            # remove PCH_SOURCE from source list
            list(REMOVE_ITEM arg_UNPARSED_ARGUMENTS ${CMAKE_CURRENT_SOURCE_DIR}/${arg_PCH_SOURCE})
            list(REMOVE_ITEM arg_UNPARSED_ARGUMENTS ${arg_PCH_SOURCE})

            if(MSVC)
                set(pch_output ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${arg_PCH_SOURCE}.pch)
                set_property(SOURCE ${arg_UNPARSED_ARGUMENTS} APPEND_STRING PROPERTY COMPILE_FLAGS "/Yu${arg_PCH_HEADER} /Fp${pch_output}")
                set_property(SOURCE ${arg_PCH_SOURCE} APPEND_STRING PROPERTY COMPILE_FLAGS "/Yc${arg_PCH_HEADER} /Fp${pch_output}")
            else()
                # todo: gcc support
            endif()
        endif()
    else()
        # TODO: GCC
    endif()
endfunction()

# Setup common C++ warning checks
#
# Usage:
# PH_setup_common_build_flags(target)
#
function(PH_setup_common_build_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    elseif(ANDROID)
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror
            -Wundef # can't use #if on undefined macro

            # -Wconversion # can't implicit conver to narrower type.
            -Wno-missing-field-initializers # we do this a lot when initializing VK structure.
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror
            -Wundef # can't use #if on undefined macro

            # -Wconversion # can't implicit conver to narrower type.
            -Wno-missing-field-initializers # we do this a lot when initializing VK structure.
            -Wno-class-memaccess
        )
    endif()
endfunction()

# Build a new PhysRay executable.
# Also setup common compile options for the executable
#
# Usage:
# PH_add_executable(target [sources...])
#
function(PH_add_executable target)
    add_executable(${target} ${ARGN})
    PH_setup_common_build_flags(${target})
endfunction()

# Build a new PhysRay library (shared or static, depending on PH_BUILD_STATIC option).
# Also setup common compile options for the library
#
# Usage:
# PH_add_library(target [sources...])
#
function(PH_add_library target)
    add_library(${target} STATIC ${ARGN})
    PH_setup_common_build_flags(${target})
endfunction()

# Look up system provided GLSLC executable
#
function(PH_get_glslc_path glslc)
    if(ANDROID)
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
            set(candidate ${ANDROID_NDK}/shader-tools/windows-x86_64/glslc.exe)
        elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
            set(candidate ${ANDROID_NDK}/shader-tools/linux-x86_64/glslc)
        else()
            message(FATAL_ERROR "Unsupported host platform. We currently only support Linux and Windows.")
            return()
        endif()

        set(${glslc} ${candidate} PARENT_SCOPE)
        PH_print_status("Use GLSLC from NDK: ${candidate}")
    elseif(${CMAKE_VERSION} VERSION_LESS "3.19.5")
        if(WIN32)
            # look for glslc
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                find_program(Vulkan_GLSLC_EXECUTABLE
                    NAMES glslc
                    PATHS "$ENV{VULKAN_SDK}/Bin")
            elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
                find_program(Vulkan_GLSLC_EXECUTABLE
                    NAMES glslc
                    PATHS "$ENV{VULKAN_SDK}/Bin32")
            endif()
        else()
            find_program(Vulkan_GLSLC_EXECUTABLE
                NAMES glslc
                PATHS "$ENV{VULKAN_SDK}/bin")
        endif()

        set(${glslc} ${Vulkan_GLSLC_EXECUTABLE} PARENT_SCOPE)
        PH_print_status("Use GLSLC found on PATH: ${Vulkan_GLSLC_EXECUTABLE}")
    else()
        # cmake 3.19.5+ already has Vulkan_GLSLC_EXECUTABLE defined
        set(${glslc} ${Vulkan_GLSLC_EXECUTABLE} PARENT_SCOPE)
        PH_print_status("Use CMake built-in GLSLC: ${Vulkan_GLSLC_EXECUTABLE}")
    endif()
endfunction()

# Compile GLSL shaders via glslc. Return list of SPIRV files via the first argument.
#
# Usage:
# PH_compile_glsl_shaders(
# targets
# SOURCES sources[...]
# [DEFINES macro[...]]
# [GLSLC full_path_to_glslc]
# [OPT on/off]
# )
#
# Options:
# targets
# Specify the variable that contains list of the compiled SPIRV files.
#
# SOURCES
# Specify GLSL source files to compile
#
# DEFINES
# Specify list of compile time defines.
#
# GLSLC
# Specify full path to the glslc executable. If omit, use system default: ${Vulkan_GLSLC_EXECUTABLE}
#
function(PH_compile_glsl_shaders targets)
    # parse the GLSLC argument. If omit, use system default.
    # set(oneValueArgs GLSLC OPT MTK)
    set(multiValueArgs SOURCES DEFINES)
    cmake_parse_arguments(arg "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(DEFINED arg_GLSLC)
        set(glslc ${arg_GLSLC})
    else()
        PH_get_glslc_path(glslc)
    endif()

    if(DEFINED arg_DEFINES)
        list(JOIN ${arg_DEFINES} " " defines)
    endif()

    # PH_print_status("defines = ${defines}")

    # force DEFFILE policy to use NEW behavior.
    if(POLICY CMP0116)
        cmake_policy(SET CMP0116 NEW)
    endif()

    foreach(glsl ${arg_SOURCES})
        get_filename_component(source_fullpath ${glsl} ABSOLUTE)
        file(RELATIVE_PATH source_relpath ${CMAKE_CURRENT_SOURCE_DIR} ${source_fullpath})
        set(spirv_fullpath ${CMAKE_CURRENT_BINARY_DIR}/${source_relpath}.spirv)
        get_filename_component(spirv_folder ${spirv_fullpath} DIRECTORY)

        if(unpatched_appendix)
            set(patch_spirv COMMAND ${spvc} -i ${spirv_fullpath}${unpatched_appendix} -o ${spirv_fullpath})
        else()
            set(patch_spirv) # do nothing
        endif()

        # (Note (05/15/2022): temporarily disable code path of Ninja based shader compileation, since it triggers shader recompliation on every build
        # regardless if shader files are modified or not)
        # if (${CMAKE_GENERATOR} STREQUAL Ninja)
        # add_custom_command(
        # OUTPUT ${spirv_fullpath}
        # DEPENDS ${source_fullpath}
        # #COMMAND ${CMAKE_COMMAND} -E echo "Compile ${source_fullpath}"
        # COMMAND ${CMAKE_COMMAND} -E make_directory "${spirv_folder}"
        # COMMAND ${glslc}
        # ${source_fullpath}
        # -o ${spirv_fullpath}${unpatched_appendix}
        # -g
        # -O
        # ${defines}
        # -MD -MF ${spirv_fullpath}.dep
        # DEPFILE ${spirv_fullpath}.dep
        # ${patch_spirv}
        # )
        # else()
        # Let the shader depends on all .glsl files in the same folder. This is to work around
        # MSBuild and Makefile limitations of not supporting the -MD -MF arguments.
        #
        # Another alternative to this is:
        # file(TOUCH_NOCREATE  ${source_fullpath})
        # That's force rebuild of all shaders all the time.
        get_filename_component(source_folder ${source_fullpath} DIRECTORY)
        file(GLOB extra_dependencies ${source_folder}/*.glsl)

        add_custom_command(
            OUTPUT ${spirv_fullpath}
            DEPENDS ${source_fullpath} ${extra_dependencies}

            # COMMAND ${CMAKE_COMMAND} -E echo "Compile ${source_fullpath}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${spirv_folder}"
            COMMAND ${glslc}
            ${source_fullpath}
            -o ${spirv_fullpath}${unpatched_appendix}
            -g
            -O
            ${defines}
            ${patch_spirv}
        )

        # endif()
        list(APPEND spirv_list ${spirv_fullpath})
    endforeach()

    set(${targets} ${spirv_list} PARENT_SCOPE)
endfunction()

# ======================================================================================================================
# import sdk libraries
# ======================================================================================================================

# Define SDK PATH variables
get_filename_component(PH_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/sdk ABSOLUTE)
if(ANDROID)
    set(PH_SDK_LIB_PATH_DEBUG   ${PH_SDK_PATH}/android.Debug/lib/${ANDROID_ABI})
    set(PH_SDK_LIB_PATH ${PH_SDK_PATH}/android.Relese/lib/${ANDROID_ABI})
elseif(WIN32)
    set(PH_SDK_LIB_PATH_DEBUG ${PH_SDK_PATH}/win64.Debug/lib)
    set(PH_SDK_LIB_PATH ${PH_SDK_PATH}/win64.Release/lib)
    set(PH_SDK_BIN_PATH_DEBUG ${PH_SDK_PATH}/win64.Debug/bin)
    set(PH_SDK_BIN_PATH ${PH_SDK_PATH}/win64.Release/bin)
else()
    set(PH_SDK_LIB_PATH_DEBUG ${PH_SDK_PATH}/linux64.Debug/lib)
    set(PH_SDK_LIB_PATH ${PH_SDK_PATH}/linux64.Release/lib)
endif()

message("

    CMAKE_BUILD_TYPE       = ${CMAKE_BUILD_TYPE}
    PH_SDK_PATH            = ${PH_SDK_PATH}
    PH_SDK_LIB_PATH        = ${PH_SDK_LIB_PATH}
    PH_SDK_LIB_PATH_DEBUG  = ${PH_SDK_LIB_PATH_DEBUG}

") # build the sample folder

# import base library
add_library(physray-base STATIC IMPORTED)
set_target_properties(physray-base
    PROPERTIES
    IMPORTED_LOCATION ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-base${CMAKE_STATIC_LIBRARY_SUFFIX}
    IMPORTED_LOCATION_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-base${CMAKE_STATIC_LIBRARY_SUFFIX}
)
target_include_directories(physray-base INTERFACE ${PH_SDK_PATH}/include)
target_compile_features(physray-base INTERFACE cxx_std_17)

if(ANDROID)
    target_link_libraries(physray-base INTERFACE android log) # dl z
endif()

# import va library
add_library(physray-va STATIC IMPORTED)
set_target_properties(physray-va
    PROPERTIES
    IMPORTED_LOCATION ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-va${CMAKE_STATIC_LIBRARY_SUFFIX}
    IMPORTED_LOCATION_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-va${CMAKE_STATIC_LIBRARY_SUFFIX}
)
target_link_libraries(physray-va INTERFACE physray-base)

# Setup Vulkan dependencies
find_package(Vulkan REQUIRED)
target_link_libraries(physray-va INTERFACE Vulkan::Vulkan)
PH_print_status("Use Vulkan SDK at ${Vulkan_INCLUDE_DIRS}")

# # import rt-asset library
# add_library(physray-rt-asset STATIC IMPORTED)
# set_target_properties(physray-rt-asset
#     PROPERTIES
#     IMPORTED_LOCATION ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-asset${CMAKE_STATIC_LIBRARY_SUFFIX}
#     IMPORTED_LOCATION_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-asset${CMAKE_STATIC_LIBRARY_SUFFIX}
# )

# # import rt-shared library
# add_library(physray-rt-shared SHARED IMPORTED)
# set_target_properties(physray-rt-shared
#     PROPERTIES
#     IMPORTED_IMPLIB ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-shared${CMAKE_STATIC_LIBRARY_SUFFIX}
#     IMPORTED_IMPLIB_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-shared${CMAKE_STATIC_LIBRARY_SUFFIX}
#     IMPORTED_LOCATION ${PH_SDK_BIN_PATH}/${CMAKE_SHARED_LIBRARY_PREFIX}physray-rt-shared${CMAKE_SHARED_LIBRARY_SUFFIX}
#     IMPORTED_LOCATION_DEBUG ${PH_SDK_BIN_PATH_DEBUG}/${CMAKE_SHARED_LIBRARY_PREFIX}physray-rt-shared${CMAKE_SHARED_LIBRARY_SUFFIX}
# )


# # import rt library
# add_library(physray-rt STATIC IMPORTED)
# set_target_properties(physray-rt
#     PROPERTIES
#     IMPORTED_LOCATION ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt${CMAKE_STATIC_LIBRARY_SUFFIX}
#     IMPORTED_LOCATION_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt${CMAKE_STATIC_LIBRARY_SUFFIX}
# )
# target_link_libraries(physray-rt INTERFACE physray-va physray-rt-asset physray-rt-shared)

# import rps library
add_library(physray-rps STATIC IMPORTED)
set_target_properties(physray-rps
    PROPERTIES
    IMPORTED_LOCATION ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rps${CMAKE_STATIC_LIBRARY_SUFFIX}
    IMPORTED_LOCATION_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rps${CMAKE_STATIC_LIBRARY_SUFFIX}
)
target_link_libraries(physray-rps INTERFACE physray-va)

# import rt-scene library
add_library(physray-rt-scene STATIC IMPORTED)
set_target_properties(physray-rt-scene
    PROPERTIES
    IMPORTED_LOCATION ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-scene${CMAKE_STATIC_LIBRARY_SUFFIX}
    IMPORTED_LOCATION_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-scene${CMAKE_STATIC_LIBRARY_SUFFIX}
)
target_link_libraries(physray-rt-scene INTERFACE physray-va)

# import rt-render library
add_library(physray-rt-render STATIC IMPORTED)
set_target_properties(physray-rt-render
    PROPERTIES
    IMPORTED_LOCATION ${PH_SDK_LIB_PATH}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-render${CMAKE_STATIC_LIBRARY_SUFFIX}
    IMPORTED_LOCATION_DEBUG ${PH_SDK_LIB_PATH_DEBUG}/${CMAKE_STATIC_LIBRARY_PREFIX}physray-rt-render${CMAKE_STATIC_LIBRARY_SUFFIX}
)
target_link_libraries(physray-rt-render INTERFACE physray-va)

# ======================================================================================================================
# build samples
# ======================================================================================================================
include(CMakeRC.cmake)
add_compile_definitions(_SILENCE_CXX20_CISO646_REMOVED_WARNING)
add_subdirectory(sample/src)
