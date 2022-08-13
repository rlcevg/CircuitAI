/*
 * Region.h
 *
 *  Created on: May 27, 2022
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_REGION_H_
#define SRC_CIRCUIT_UTIL_MATH_REGION_H_

#include "util/Defines.h"

#include <vector>

namespace utils {

union SBox {
	SBox(float l, float r, float t, float b) : edge{l, r, t, b} {}
	SBox() : edge{0.f, 0.f, 0.f, 0.f} {}
	struct {
		float left;
		float right;
		float top;
		float bottom;
	};
	float edge[4];

	bool ContainsPoint(const springai::AIFloat3& point) const {
		return (point.x >= left) && (point.x <= right) && (point.z >= top) && (point.z <= bottom);
	}
	void Merge(const SBox& box) {
		left   = std::min(left,   box.left);
		right  = std::max(right,  box.right);
		top    = std::min(top,    box.top);
		bottom = std::max(bottom, box.bottom);
	}
	void Add(const springai::AIFloat3& point) {
		left   = std::min(left,   point.x);
		right  = std::max(right,  point.x);
		top    = std::min(top,    point.z);
		bottom = std::max(bottom, point.z);
	}
};

class CPolygon {
public:
	CPolygon(F3Vec&& points);
	CPolygon(const SBox& b);
	~CPolygon() {}

	const SBox& GetBox() const { return box; }  // bounding box
	float GetArea() const { return area; }
	bool ContainsPoint(const springai::AIFloat3& point) const;
	springai::AIFloat3 Random() const;
	void Scale(float value);
	void Extend(float value);

private:
	void CalcArea();

	SBox box;
	F3Vec verts;
	IndexVec tris;
	FloatVec areas;
	float area;
	springai::AIFloat3 center;
};

class CRegion {
public:
	CRegion() : area(0.f) {}  // for easy use with std::map
	CRegion(std::vector<CPolygon>&& polys);
	CRegion(SBox&& b);
	~CRegion() {}

	const SBox& GetBox() const { return box; }  // bounding box
	bool ContainsPoint(const springai::AIFloat3& point) const;
	springai::AIFloat3 Random() const;

private:
	SBox box;
	std::vector<CPolygon> parts;
	float area;
};

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_MATH_REGION_H_
