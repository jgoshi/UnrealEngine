/*
 * Copyright (c) 2008-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.

#pragma once

#include "Types.h"

namespace nvidia
{
namespace cloth
{

struct PhaseConfig
{
	PhaseConfig(uint16_t index = uint16_t(-1));

	uint16_t mPhaseIndex;
	uint16_t mPadding;

	// target convergence rate per iteration (1/solverFrequency)
	float mStiffness;

	float mStiffnessMultiplier;

	float mCompressionLimit;
	float mStretchLimit;
};

} // namespace cloth
} // namespace nvidia
