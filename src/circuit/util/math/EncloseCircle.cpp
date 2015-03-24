/*
 * EncloseCircle.cpp
 *
 *  Created on: Mar 23, 2015
 *      Author: rlcevg
 */

#include "util/math/EncloseCircle.h"
#include "util/utils.h"

#include <algorithm>
#include <assert.h>

namespace circuit {

using namespace springai;

CEncloseCircle::CEncloseCircle()
{
}

CEncloseCircle::~CEncloseCircle()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

/*
 * Returns the smallest circle that encloses all the given points. Runs in expected O(n) time, randomized.
 * Note: If 0 points are given, null is returned. If 1 point is given, a circle of radius 0 is returned.
 */
// Initially: No boundary points known
void CEncloseCircle::MakeCircle(const std::vector<AIFloat3>& points)
{
	assert(!points.empty());

	SCircle circle;
	if (points.size() < 2) {
		circle = SCircle(SPoint(points[0].x, points[0].z), 0);
	} else if (points.size() < 3) {
		circle = MakeDiameter(SPoint(points[0].x, points[0].z), SPoint(points[1].x, points[1].z));
	} else {
		// Clone list to preserve the caller's data
		std::vector<SPoint> shuffled;
		shuffled.reserve(points.size());
		for (auto& pos : points) {
			shuffled.push_back(SPoint(pos.x, pos.z));
		}
		// Randomize order
		// TODO: Read why shuffle??
		std::random_shuffle(shuffled.begin(), shuffled.end());

		// Progressively add points to circle or recompute circle
		decltype(shuffled)::iterator it = shuffled.begin();
		circle = MakeCircleOnePoint(shuffled.begin(), ++it, shuffled[0]);
		while (it != shuffled.end()) {
			const SPoint& p = *it++;
			if (!circle.contains(p)) {
				circle = MakeCircleOnePoint(shuffled.begin(), it, p);
			}
		}
	}

	center = AIFloat3(circle.c.x, 0, circle.c.y);
	radius = circle.r;
}

const AIFloat3& CEncloseCircle::GetCenter() const
{
	return center;
}

float CEncloseCircle::GetRadius() const
{
	return radius;
}

// One boundary point known
CEncloseCircle::SCircle CEncloseCircle::MakeCircleOnePoint(const std::vector<SPoint>::iterator& ptsBegin,
										   const std::vector<SPoint>::iterator& ptsEnd, const SPoint& p)
{
	// ptsBegin - Inclusive, ptsEnd - Exclusive
	SCircle c(p, 0);
	auto it = ptsBegin;
	while (it != ptsEnd) {
		const SPoint& q = *it++;
		if (!c.contains(q)) {
			if (c.r == 0) {
				c = MakeDiameter(p, q);
			} else {
				c = MakeCircleTwoPoints(ptsBegin, it, p, q);
			}
		}
	}
	return c;
}


// Two boundary points known
CEncloseCircle::SCircle CEncloseCircle::MakeCircleTwoPoints(const std::vector<SPoint>::iterator& ptsBegin,
											const std::vector<SPoint>::iterator& ptsEnd, const SPoint& p, const SPoint& q)
{
	SCircle temp = MakeDiameter(p, q);
	if (temp.contains(ptsBegin, ptsEnd)) {
		return temp;
	}

	SCircle left, right;
	bool hasLeft = false, hasRight = false;
	auto it = ptsBegin;
	while (it != ptsEnd) {  // Form a circumcircle with each point
		const SPoint& r = *it++;
		SPoint pq = q - p;
		float cross = pq.cross(r - p);
		SCircle c;
		if (!MakeCircumcircle(p, q, r, c)) {
			continue;
		} else if ((cross > 0) && (!hasLeft || (pq.cross(c.c - p) > pq.cross(left.c - p)))) {
			left = c;
			hasLeft = true;
		} else if ((cross < 0) && (!hasRight || (pq.cross(c.c - p) < pq.cross(right.c - p)))) {
			right = c;
			hasRight = true;
		}
	}
	return (!hasRight || (hasLeft && (left.r <= right.r))) ? left : right;
}

CEncloseCircle::SCircle CEncloseCircle::MakeDiameter(const SPoint& a, const SPoint& b)
{
	return SCircle(SPoint((a.x + b.x)/ 2, (a.y + b.y) / 2), a.distance(b) / 2);
}

bool CEncloseCircle::MakeCircumcircle(const SPoint& a, const SPoint& b, const SPoint& c, SCircle& circle)
{
	// Mathematical algorithm from Wikipedia: Circumscribed circle
	float d = (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y)) * 2;
	if (d == 0) {
		return false;
	}
	float x = (a.norm() * (b.y - c.y) + b.norm() * (c.y - a.y) + c.norm() * (a.y - b.y)) / d;
	float y = (a.norm() * (c.x - b.x) + b.norm() * (a.x - c.x) + c.norm() * (b.x - a.x)) / d;
	circle.c = SPoint(x, y);
	circle.r = circle.c.distance(a);
	return true;
}

} // namespace circuit
