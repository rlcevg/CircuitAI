/*
 * TerrainData.h
 *
 *  Created on: Dec 15, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAINDATA_H_
#define SRC_CIRCUIT_TERRAINDATA_H_

#include "AIFloat3.h"

#include <vector>
#include <atomic>
#include <mutex>

namespace circuit {

// debug
class CCircuitAI;

class CTerrainData {
public:
	CTerrainData();
	virtual ~CTerrainData();

	bool IsClusterizing();
	void SetClusterizing(bool value);

	void ClusterLock();
	void ClusterUnlock();
	const std::vector<springai::AIFloat3>& GetDefencePoints() const;
	const std::vector<springai::AIFloat3>& GetDefencePerimeter() const;

	void Clusterize(const std::vector<springai::AIFloat3>& wayPoints, float maxDistance, CCircuitAI* circuit);

	// debug, could be used for defence perimeter calculation
//	void DrawConvexHulls(springai::Drawer* drawer);
//	void DrawCentroids(springai::Drawer* drawer);
//	void ClearMetalClusters(springai::Drawer* drawer);

private:
	std::vector<springai::AIFloat3> points0;
	std::vector<springai::AIFloat3> points1;
	std::atomic<std::vector<springai::AIFloat3>*> ppoints;

	std::atomic<bool> isClusterizing;
	std::mutex clusterMutex;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAINDATA_H_
