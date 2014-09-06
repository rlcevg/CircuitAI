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
#include <atomic>
#include <mutex>

namespace springai {
	class Drawer;
}

namespace circuit {

using Metal = struct Metal {
	float income;
	springai::AIFloat3 position;
};
using Metals = std::vector<Metal>;

class CMetalManager {
public:
	CMetalManager(std::vector<Metal>& spots);
	virtual ~CMetalManager();

	bool IsEmpty();
	bool IsClusterizing();
	void SetClusterizing(bool value);
	std::vector<Metal>& GetSpots();
	std::vector<Metals>& GetClusters();

	/*
	 * Hierarchical clusterization. Not reusable. Metric: complete link
	 */
	void Clusterize(float maxDistance, float** distmatrix);
	void DrawConvexHulls(springai::Drawer* drawer);
//	void DrawCentroids(springai::Drawer* drawer);
	void ClearMetalClusters(springai::Drawer* drawer);

private:
	std::vector<Metal> spots;
	// Double buffer clusters as i don't want to copy vectors every time for safe use
	std::vector<Metals> clusters0;
	std::vector<Metals> clusters1;
	std::vector<Metals>* pclusters;
//	std::vector<springai::AIFloat3> centroids;
	std::atomic<bool> isClusterizing;
	std::mutex clusterMutex;
};

} // namespace circuit

#endif // METALMANAGER_H_
