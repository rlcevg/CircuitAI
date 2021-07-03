/*
 * Point.h
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_POINT_H_
#define SRC_CIRCUIT_UTIL_POINT_H_

#include "Sim/Misc/GlobalConstants.h"
#include "AIFloat3.h"

namespace utils {

static inline bool is_equal_pos(const springai::AIFloat3& posA, const springai::AIFloat3& posB, const float slack = SQUARE_SIZE * 2)
{
	return (math::fabs(posA.x - posB.x) <= slack) && (math::fabs(posA.z - posB.z) <= slack);
}

static inline springai::AIFloat3 get_near_pos(const springai::AIFloat3& pos, float range)
{
	springai::AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.f, (float)rand() / RAND_MAX - 0.5f);
	return pos + offset * range;
}

static inline springai::AIFloat3 get_radial_pos(const springai::AIFloat3& pos, float radius)
{
	float angle = (float)rand() / RAND_MAX * 2 * M_PI;
	springai::AIFloat3 offset(std::cos(angle), 0.f, std::sin(angle));
	return pos + offset * radius;
}

static inline bool is_valid(const springai::AIFloat3& pos)
{
	return pos.x != -RgtVector.x;
}

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_POINT_H_
