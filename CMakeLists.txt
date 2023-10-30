#############################################################################
# Copyright (C) 2020 - 2023 OPPO. All rights reserved.
###############################################################################

cmake_minimum_required(VERSION 3.16.3) # 3.16.3 is the latest cmake version coming with Ubuntu 20.04

project(physray-sdk)

add_definitions(-D_SILENCE_CXX20_CISO646_REMOVED_WARNING)
add_definitions(-D_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS)

# The following code was added to force the visibility of the symbols to 'hidden' by default on ALL platforms. On
# Windows, this is the default value, but on POSIX platforms (such as Linux or Android), and depending on the compiler
# (GCC, Clang, ...) the default visibility is 'default' or 'visible', meaning that all the symbols will be exposed
# automatically if we build a shared library.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
# Suppress warnings from submodules introduced by the above symbol visibility change
set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# ======================================================================================================================
# Build Options
# ======================================================================================================================

# define build options
option(PH_BUILD_SAMPLE        "Build sample folder" ON)
option(PH_BUILD_DEV           "Build dev folder (only needed for SDK developers)" ON)
option(PH_NINJA_COLOR_OUTPUT  "Turn on compiler color output with ninja build" ON)

# ======================================================================================================================
# Define common build options and flags
# ======================================================================================================================

macro(PH_print_status)
    message("[PhysRay-CMAKE] ${ARGN}")
endmacro()

macro(PH_warning)
    message(WARNING "========================\n\n[PhysRay-CMAKE] ${ARGN}\n\n========================")
endmacro()

# Setup default build type. Note that his variable is only meaningful to single-configuration generators
# (such as Makefile Generators and Ninja). It has no effect on Visual Studio build.
if ("${CMAKE_BUILD_TYPE} " STREQUAL " ")
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Print build information
PH_print_status("CMAKE_VERSION         = ${CMAKE_VERSION}")
PH_print_status("CMAKE_SYSTEM_NAME     = ${CMAKE_SYSTEM_NAME}")
PH_print_status("CMAKE_GENERATOR       = ${CMAKE_GENERATOR}")
PH_print_status("CMAKE_BUILD_TYPE      = ${CMAKE_BUILD_TYPE}")
PH_print_status("CMAKE_SIZEOF_VOID_P   = ${CMAKE_SIZEOF_VOID_P}")
PH_print_status("PH_BUILD_SAMPLE       = ${PH_BUILD_SAMPLE}")
PH_print_status("PH_BUILD_DEV          = ${PH_BUILD_DEV}")
PH_print_status("PH_NINJA_COLOR_OUTPUT = ${PH_NINJA_COLOR_OUTPUT}")

# if (NOT Ninja STREQUAL ${CMAKE_GENERATOR})
#     message(WARNING "
# PhysRay-SDK cmake files relies on Ninja build system to detect include depencencies among GLSL shader sources.
# Consider switch to Ninja (`cmake -GNinja ..`) for more reliable build experience for GLSL shaders.
# ")
# endif()

# ======================================================================================================================
# utility functions
# ======================================================================================================================

# Setup PCH for source files
#
# Usage:
#    PH_setup_pch(source1 [source2 ...] PCH_SOURCE pch_source [PCH_HEADER pch_header])
#
# Default pch header is pch.h, if not specified.
# ------------------------------------------------------------------------------
function(PH_setup_pch)
    string(FIND ${CMAKE_MAKE_PROGRAM} MSBuild isMsBuild)
    if (${isMsBuild} GREATER 0)
        set(oneValueArgs PCH_SOURCE PCH_HEADER)
        cmake_parse_arguments(arg "" "${oneValueArgs}" "" ${ARGN})
        if (arg_UNPARSED_ARGUMENTS AND arg_PCH_SOURCE)
            if (NOT arg_PCH_HEADER)
                set(arg_PCH_HEADER pch.h)
            endif()
            # remove PCH_SOURCE from source list
            list(REMOVE_ITEM arg_UNPARSED_ARGUMENTS ${CMAKE_CURRENT_SOURCE_DIR}/${arg_PCH_SOURCE})
            list(REMOVE_ITEM arg_UNPARSED_ARGUMENTS ${arg_PCH_SOURCE})
            if (MSVC)
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
#   PH_setup_common_build_flags(target)
#
function(PH_setup_common_build_flags target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /MP)
    elseif(ANDROID)
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror
            -Wundef      # can't use #if on undefined macro
           #-Wconversion # can't implicit conver to narrower type.
            -Wno-missing-field-initializers # we do this a lot when initializing VK structure.
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror
            -Wundef      # can't use #if on undefined macro
           #-Wconversion # can't implicit conver to narrower type.
            -Wno-missing-field-initializers # we do this a lot when initializing VK structure.
            -Wno-class-memaccess
        )
    endif()
endfunction()

# Build a new PhysRay executable.
# Also setup common compile options for the executable
#
# Usage:
#   PH_add_executable(target [sources...])
#
function(PH_add_executable target)
    add_executable(${target} ${ARGN})
    PH_setup_common_build_flags(${target})
endfunction()

# Build a new PhysRay static library
# Also setup common compile options for the library
#
# Usage:
#   PH_add_library(target [sources...])
#
function(PH_add_library target)
    add_library(${target} STATIC ${ARGN})
    PH_setup_common_build_flags(${target})
endfunction()

# Build a new PhysRay shared library.
# Also setup common compile options for the library
#
# Usage:
#   PH_add_shared_library(target [sources...])
#
function(PH_add_shared_library target)
    add_library(${target} SHARED ${ARGN})
    PH_setup_common_build_flags(${target})
endfunction()

# Look up system provided GLSLC executable
#
function(PH_get_glslc_path glslc)
    if (ANDROID)
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
            set(candidate ${ANDROID_NDK}/shader-tools/windows-x86_64/glslc.exe)
        elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
            set(candidate ${ANDROID_NDK}/shader-tools/linux-x86_64/glslc)
        else()
            message(FATAL_ERROR "Unsupported host platform. We currently only support Linux and Windows.")
            return()
        endif()
        set(${glslc} ${candidate} PARENT_SCOPE)
        PH_print_status("Use GLSLC from NDK: ${candidate}")
    elseif (${CMAKE_VERSION} VERSION_LESS "3.19.5")
        if (WIN32)
            # look for glslc
            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                find_program(Vulkan_GLSLC_EXECUTABLE
                    NAMES glslc
                    PATHS "$ENV{VULKAN_SDK}/Bin")
            elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
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
#   PH_compile_glsl_shaders(
#           targets
#           SOURCES sources[...]
#           [DEFINES macro[...]]
#           [GLSLC full_path_to_glslc]
#           [OPT on/off]
#   )
#
# Options:
#   targets
#       Specify the variable that contains list of the compiled SPIRV files.
#
#   SOURCES
#       Specify GLSL source files to compile
#
#   DEFINES
#       Specify list of compile time defines.
#
#   GLSLC
#       Specify full path to the glslc executable. If omit, use system default: ${Vulkan_GLSLC_EXECUTABLE}
#
#   OPT
#	Turn on/off shader optimization. Default is on

function(PH_compile_glsl_shaders targets)
    # parse the GLSLC argument. If omit, use system default.
    set(oneValueArgs GLSLC OPT)
    set(multiValueArgs SOURCES DEFINES)
    cmake_parse_arguments(arg "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if (DEFINED arg_GLSLC)
        set(glslc ${arg_GLSLC})
    else()
        PH_get_glslc_path(glslc)
    endif()
    # PH_print_status("compile ${target} with GLSL compiler: ${glslc}")

    if (DEFINED arg_DEFINES)
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

        if (unpatched_appendix)
            set(patch_spirv COMMAND ${spvc}  -i ${spirv_fullpath}${unpatched_appendix} -o ${spirv_fullpath})
        else()
            set(patch_spirv) # do nothing
        endif()

        # (Note (05/15/2022): temporarily disable code path of Ninja based shader compileation, since it triggers shader recompliation on every build
        # regardless if shader files are modified or not)
        # if (${CMAKE_GENERATOR} STREQUAL Ninja)
        #     add_custom_command(
        #         OUTPUT ${spirv_fullpath}
        #         DEPENDS ${source_fullpath}
        #         #COMMAND ${CMAKE_COMMAND} -E echo "Compile ${source_fullpath}"
        #         COMMAND ${CMAKE_COMMAND} -E make_directory "${spirv_folder}"
        #         COMMAND ${glslc}
        #             ${source_fullpath}
        #             -o ${spirv_fullpath}${unpatched_appendix}
        #             -g
        #             -O
        #             ${defines}
        #             -MD -MF ${spirv_fullpath}.dep
        #         DEPFILE ${spirv_fullpath}.dep
        #         ${patch_spirv}
        #     )
        # else()
            # Let the shader depends on all .glsl files in the same folder. This is to work around
            # MSBuild and Makefile limitations of not supporting the -MD -MF arguments.
            #
            # Another alternative to this is:
            #       file(TOUCH_NOCREATE  ${source_fullpath})
            # That's force rebuild of all shaders all the time.
            get_filename_component(source_folder ${source_fullpath} DIRECTORY)
            file(GLOB extra_dependencies ${source_folder}/*.glsl)

            add_custom_command(
                OUTPUT ${spirv_fullpath}
                DEPENDS ${source_fullpath} ${extra_dependencies}
                #COMMAND ${CMAKE_COMMAND} -E echo "Compile ${source_fullpath}"
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
# Setup global compiler options for the whole repo
# ======================================================================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# Enable code optimiation in debug build to improve debug build perf.
if (MSVC)
    # We don't call add_compile_options() here since we also need to
    # remove /Od and /RTC1 options from the debug build flag.
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Ob0 /O2")
endif()

if (PH_NINJA_COLOR_OUTPUT)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif()
endif()

# ======================================================================================================================
# Build subfolders
# ======================================================================================================================

add_subdirectory(sdk)

if (PH_BUILD_SAMPLE)
    add_subdirectory(sample)
endif()

if (PH_BUILD_DEV)
    add_subdirectory(dev)
endif()
