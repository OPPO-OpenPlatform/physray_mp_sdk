#############################################################################
# Copyright (C) 2020 - 2023 OPPO. All rights reserved.
###############################################################################

add_subdirectory(rt/common)
if (NOT ANDROID)
    add_subdirectory(desktop)
endif()