/*
 * ConvexHull.h
 *
 *  Created on: Feb 12, 2022
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MATH_CONVEXHULL_H_
#define SRC_CIRCUIT_UTIL_MATH_CONVEXHULL_H_

#include "resource/MetalData.h"

namespace springai {
	class Drawer;
}

namespace utils {

class CConvexHull {
public:
	CConvexHull(const circuit::CMetalData::Metals& spots, const circuit::CMetalData::Clusters& clusters);
	~CConvexHull();

	void DrawConvexHulls(springai::Drawer* drawer);

private:
	circuit::CMetalData::Metals spots;
	circuit::CMetalData::Clusters clusters;
};

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_MATH_CONVEXHULL_H_
