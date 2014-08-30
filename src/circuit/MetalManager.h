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

namespace circuit {

#define DEFAULT_MEXPERCLUSTER	3

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

public:
//	void SortSpotsRadial();

	std::vector<Metal> spots;
	std::vector<std::vector<Metal>> clusters;
	std::vector<springai::AIFloat3> centroids;
	int mexPerClusterAvg;
};

} // namespace circuit

#endif // METALMANAGER_H_
