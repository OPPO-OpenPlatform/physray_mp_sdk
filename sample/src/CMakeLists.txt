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

add_subdirectory(rt/common)
if (NOT ANDROID)
    add_subdirectory(desktop)
endif()