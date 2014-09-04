/*
 * MetalManager.h
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#ifndef METALMANAGER_H_
#define METALMANAGER_H_

#include "AIFloat3.h"

#include <vector>

namespace springai {
	class Pathing;
	class Map;
}

namespace circuit {

using Metal = struct Metal {
	float income;
	springai::AIFloat3 position;
};

class CMetalManager {
public:
	CMetalManager(std::vector<Metal>& spots);
	virtual ~CMetalManager();

	bool IsEmpty();
	std::vector<Metal>& GetSpots();

	/*
	 * Hierarchical clusterization without saving tree.
	 */
	void Clusterize(float maxDistance, int pathType, springai::Pathing* pathing);
	void DrawConvexHulls(springai::Map* map);
//	void DrawCentroids(springai::Map* map);
	void ClearMetalClusters(springai::Map* map);

private:
	std::vector<Metal> spots;
	std::vector<std::vector<Metal>> clusters;
//	std::vector<springai::AIFloat3> centroids;
};

} // namespace circuit

#endif // METALMANAGER_H_
