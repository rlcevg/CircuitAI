/*
 * MetalSpotManager.h
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#ifndef METALSPOTMANAGER_H_
#define METALSPOTMANAGER_H_

#include "AIFloat3.h"

#include <vector>

namespace circuit {

#define DEFAULT_MEXPERCLUSTER	3

using Metal = struct Metal {
	float income;
	springai::AIFloat3 position;
};

class CMetalSpotManager {
public:
	CMetalSpotManager(std::vector<Metal>& spots);
	virtual ~CMetalSpotManager();

	bool IsEmpty();
	std::vector<Metal>& GetSpots();

private:
	void SortSpotsRadial();

	std::vector<Metal> spots;
	std::vector<std::vector<Metal>> clusters;
	std::vector<springai::AIFloat3> centroids;
	int mexPerClusterAvg;
};

} // namespace circuit

#endif // METALSPOTMANAGER_H_
