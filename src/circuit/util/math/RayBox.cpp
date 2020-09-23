/*
 * RayBox.cpp
 *
 *  Created on: Sep 23, 2020
 *      Editor: rlcevg
*/

#include "util/math/RayBox.h"

namespace circuit {

using namespace springai;

bool CAABBox::IntersectFront(const CRayFront &r, float &t) const
{
	float tmin, tmax, tymin, tymax, tzmin, tzmax;

	tmin = (bounds[r.sign[0]].x - r.orig.x) * r.invdir.x;
	tmax = (bounds[1-r.sign[0]].x - r.orig.x) * r.invdir.x;
	tymin = (bounds[r.sign[1]].y - r.orig.y) * r.invdir.y;
	tymax = (bounds[1-r.sign[1]].y - r.orig.y) * r.invdir.y;

	if ((tmin > tymax) || (tymin > tmax))
		return false;

	if (tymin > tmin)
		tmin = tymin;
	if (tymax < tmax)
		tmax = tymax;

	tzmin = (bounds[r.sign[2]].z - r.orig.z) * r.invdir.z;
	tzmax = (bounds[1-r.sign[2]].z - r.orig.z) * r.invdir.z;

	if ((tmin > tzmax) || (tzmin > tmax))
		return false;

	if (tzmin > tmin)
		tmin = tzmin;
	if (tzmax < tmax)
		tmax = tzmax;

	t = tmin;

	if (t < 0) {
		t = tmax;
		if (t < 0) return false;
	}

	return true;
}

bool CAABBox::Intersection(const CRay& r) const
{
	// NOTE: branchless?
	float t1 = (bounds[0].x - r.orig.x) * r.invdir.x;
	float t2 = (bounds[1].x - r.orig.x) * r.invdir.x;

	float tmin = std::min(t1, t2);
	float tmax = std::max(t1, t2);

	for (int i = 1; i < 3; ++i) {
		t1 = (bounds[0][i] - r.orig[i]) * r.invdir[i];
		t2 = (bounds[1][i] - r.orig[i]) * r.invdir[i];

#if 1  // No NaN handling
		tmin = std::max(tmin, std::min(t1, t2));
		tmax = std::min(tmax, std::max(t1, t2));
#else	// With NaN handling:
		tmin = std::max(tmin, std::min(std::min(t1, t2), tmax));
		tmax = std::min(tmax, std::max(std::max(t1, t2), tmin));
#endif
	}

	return tmax > std::max(tmin, 0.f);
}

} // namespace circuit
