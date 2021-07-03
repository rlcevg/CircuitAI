/*
 * RayBox.h
 *
 *  Ray-Box intersection
 *  Created on: Sep 23, 2020
 *      Editor: rlcevg
 *      Origin: https://tavianator.com/2015/ray_box_nan.html
 *         Alt: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
  */

#ifndef SRC_CIRCUIT_UTIL_MATH_RAYBOX_H_
#define SRC_CIRCUIT_UTIL_MATH_RAYBOX_H_

#include "AIFloat3.h"

namespace circuit {

class CRay {
public:
	CRay(const springai::AIFloat3& orig, const springai::AIFloat3& dir)
		: orig(orig), invdir(springai::AIFloat3(1, 1, 1) / dir)
	{}

	springai::AIFloat3 orig; // ray orig
	springai::AIFloat3 invdir;  // 1.f / dir
};

class CRayFront {
public:
	CRayFront(const springai::AIFloat3& orig, const springai::AIFloat3& dir) : orig(orig) {
		invdir = springai::AIFloat3(1, 1, 1) / dir;
		sign[0] = (invdir.x < 0);
		sign[1] = (invdir.y < 0);
		sign[2] = (invdir.z < 0);
	}

	springai::AIFloat3 orig; // ray orig
	springai::AIFloat3 invdir;  // 1.f / dir
	int sign[3];
};

class CAABBox {  // Class Axis-Aligned Bounding Box
public:
	CAABBox(const springai::AIFloat3& b0, const springai::AIFloat3& b1) {
		bounds[0] = b0, bounds[1] = b1;
	}
	bool IntersectFront(const CRayFront &r, float &t) const;
	bool Intersection(const CRay &r) const;

	springai::AIFloat3 bounds[2];
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MATH_RAYBOX_H_
