/*
 * EncloseCircle.h
 *
 *  Smallest enclosing circle
 *  Created on: Mar 23, 2015
 *      Author: rlcevg
 *      Origin: http://www.nayuki.io/page/smallest-enclosing-circle
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_ENCLOSECIRCLE_H_
#define SRC_CIRCUIT_UTIL_MATH_ENCLOSECIRCLE_H_

#include "AIFloat3.h"

#include <vector>

namespace circuit {

class CEncloseCircle {
private:
	struct SPoint {
		float x, y;

		SPoint() : x(0), y(0) {}
		SPoint(float x, float y) :
			x(x), y(y)
		{}

		SPoint operator- (const SPoint& p) const {
			return SPoint(x - p.x, y - p.y);
		}

		float distance(const SPoint& p) const {
			const float dx = x - p.x;
			const float dy = y - p.y;
			return math::sqrt(dx * dx + dy * dy);
		}

		// Signed area / determinant thing
		float cross(const SPoint& p) const {
			return x * p.y - y * p.x;
		}

		// Magnitude squared
		float norm() const {
			return x * x + y * y;
		}
	};

	struct SCircle {
		constexpr static float EPSILON = (1e-12);
		SPoint c;   // Center
		float r;  // Radius

		SCircle() : r(0) {}
		SCircle(const SPoint& c, float r) :
			c(c), r(r)
		{}

		bool contains(const SPoint& p) {
			return c.distance(p) <= r + EPSILON;
		}

		bool contains(const std::vector<SPoint>::iterator& ptsBegin, const std::vector<SPoint>::iterator& ptsEnd) {
			auto it = ptsBegin;
			while (it != ptsEnd) {
				if (!contains(*it++)) {
					return false;
				}
			}
			return true;
		}
	};

public:
	CEncloseCircle();
	virtual ~CEncloseCircle();

	void MakeCircle(const std::vector<springai::AIFloat3>& points);
	const springai::AIFloat3& GetCenter() const;
	float GetRadius() const;

private:
	SCircle MakeCircleOnePoint(const std::vector<SPoint>::iterator& ptsBegin,
							   const std::vector<SPoint>::iterator& ptsEnd, const SPoint& p);
	SCircle MakeCircleTwoPoints(const std::vector<SPoint>::iterator& ptsBegin,
								const std::vector<SPoint>::iterator& ptsEnd, const SPoint& p, const SPoint& q);
	SCircle MakeDiameter(const SPoint& a, const SPoint& b);
	bool MakeCircumcircle(const SPoint& a, const SPoint& b, const SPoint& c, SCircle& circle);

	springai::AIFloat3 center;
	float radius;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MATH_ENCLOSECIRCLE_H_
