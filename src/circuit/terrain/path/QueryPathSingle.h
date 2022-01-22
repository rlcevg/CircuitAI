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
			float maxRange, NSMicroPather::HitFunc&& hitTest,
			float maxThreat, bool endPosOnly);

	void Prepare();

	// Process Data
	springai::AIFloat3& GetStartPosRef() { return startPos; }
	springai::AIFloat3& GetEndPosRef() { return endPos; }
	PathInfo& GetPathInfoRef() { return *pPath; }
	float& GetPathCostRef() { return pathCost; }

	// Input Data
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	const springai::AIFloat3& GetEndPos() const { return endPos; }
	const float GetMaxRange() const { return maxRange; }
	const NSMicroPather::HitFunc& GetHitTest() const { return hitTest; }
	const float GetMaxThreat() const { return maxThreat; }

	// Result
	const std::shared_ptr<PathInfo>& GetPathInfo() const { return pPath; }
	const float GetPathCost() const { return pathCost; }

private:
	std::shared_ptr<PathInfo> pPath;
	float pathCost = 0.f;

	springai::AIFloat3 startPos;
	springai::AIFloat3 endPos;
	float maxRange = 0.f;
	NSMicroPather::HitFunc hitTest;
	float maxThreat = 0.f;
	bool endPosOnly = false;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHSINGLE_H_
