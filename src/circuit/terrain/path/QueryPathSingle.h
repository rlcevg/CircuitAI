/*
 * QueryPathSingle.h
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHSINGLE_H_
#define SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHSINGLE_H_

#include "terrain/path/PathQuery.h"

namespace circuit {

class CQueryPathSingle: public IPathQuery {
public:
	CQueryPathSingle(const CPathFinder& pathfinder, int id);
	virtual ~CQueryPathSingle();

	void InitQuery(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos,
			float maxRange, float maxThreat, bool endPosOnly);

	void Prepare();

	// Process Data
	PathInfo& GetRefPathInfo() { return *pPath; }
	float& GetRefPathCost() { return pathCost; }

	// Input Data
	springai::AIFloat3& GetStartPos() { return startPos; }
	springai::AIFloat3& GetEndPos() { return endPos; }
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
	bool endPosOnly = false;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHSINGLE_H_
