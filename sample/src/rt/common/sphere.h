/*****************************************************************************
 * Copyright (C), 2023,  Computing & Graphics Research Institute of OPLUS Mobile Comm Corp., Ltd
 * License: No license is required for Oplus internal usage.
 *          No external usage is allowed.
 *
 * File : sphere.h
 *
 * Version: 2.0
 *
 * Date : Feb 2023
 *
 * Author: Computing & Graphics Research Institute
 *
 * ------------------ Revision History: ---------------------
 *  <version>  <date>  <author>  <desc>
 *
 *******************************************************************************/

#pragma once
#include <ph/base.h>
#include <ph/va.h> // this is to include Eigen header included by va.h
#include <vector>

std::vector<Eigen::Vector3f> buildIcosahedronUnitSphere(uint32_t subdivide = 0);
