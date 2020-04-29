/*
 * QueryPathInfo.h
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHINFO_H_
#define SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHINFO_H_

#include "terrain/path/PathQuery.h"

namespace circuit {

class CQueryPathInfo: public IPathQuery {
public:
	CQueryPathInfo(const CPathFinder& pathfinder, int id);
	virtual ~CQueryPathInfo();

	void InitQuery(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos,
			float maxRange, float maxThreat);

	void Prepare();

	// Process Data
	PathInfo& GetRefPathInfo() { return *pPath; }
	float& GetRefPathCost() { return pathCost; }

	// Input Data
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	const springai::AIFloat3& GetEndPos() const { return endPos; }
	const float GetMaxRange() const { return maxRange; }
	const float GetMaxThreat() const { return maxThreat; }

	// Result
	std::shared_ptr<PathInfo> GetPathInfo() const { return pPath; }
	const float GetPathCost() const { return pathCost; }

private:
	std::shared_ptr<PathInfo> pPath;
	float pathCost = 0.f;

	springai::AIFloat3 startPos;
	springai::AIFloat3 endPos;
	float maxRange = 0.f;
	float maxThreat = 0.f;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHINFO_H_
