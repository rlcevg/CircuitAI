/*
 * Geometry.h
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_GEOMETRY_H_
#define SRC_CIRCUIT_UTIL_MATH_GEOMETRY_H_

#include "Sim/Misc/GlobalConstants.h"
#include "AIFloat3.h"

namespace utils {

static inline bool is_equal_pos(const springai::AIFloat3& posA, const springai::AIFloat3& posB, const float slack = SQUARE_SIZE * 2)
{
	return (std::fabs(posA.x - posB.x) <= slack) && (std::fabs(posA.z - posB.z) <= slack);
}

static inline springai::AIFloat3 get_near_pos(const springai::AIFloat3& pos, float range)
{
	const springai::AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.f, (float)rand() / RAND_MAX - 0.5f);
	return pos + offset * range;
}

static inline springai::AIFloat3 get_radial_pos(const springai::AIFloat3& pos, float radius)
{
	const float angle = (float)rand() / RAND_MAX * 2 * M_PI;
	springai::AIFloat3 offset(std::cos(angle), 0.f, std::sin(angle));
	return pos + offset * radius;
}

static inline bool is_valid(const springai::AIFloat3& pos)
{
	return pos.x != -RgtVector.x;
}

/*
 * nanoflann
 */
template <class Derived>
struct SPointAdaptor {
	const Derived& pts;
	SPointAdaptor(const Derived& v) : pts(v) {}
	/*
	 * KDTree adapter interface
	 */
	// Must return the number of data points
	inline size_t kdtree_get_point_count() const { return pts.size(); }
	// Returns the dim'th component of the idx'th point in the class:
		// Since this is inlined and the "dim" argument is typically an immediate value, the
		//  "if/else's" are actually solved at compile time.
	inline float kdtree_get_pt(const size_t idx, const size_t dim) const {
		return (dim == 0) ? pts[idx].position.x : pts[idx].position.z;
	}
	// Optional bounding-box computation: return false to default to a standard bbox computation loop.
		//   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
		//   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
	template <class BBOX>
	bool kdtree_get_bbox(BBOX& /* bb */) const { return false; }
};

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_MATH_GEOMETRY_H_
