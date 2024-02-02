/*****************************************************************************
 * Copyright (C) 2020 - 2024 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once
#include <ph/base.h>
#include <ph/va.h> // this is to include Eigen header included by va.h
#include <vector>

std::vector<Eigen::Vector3f> buildIcosahedronUnitSphere(uint32_t subdivide = 0);
