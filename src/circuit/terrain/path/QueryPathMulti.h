/*
 * QueryPathMulti.h
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHMULTI_H_
#define SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHMULTI_H_

#include "terrain/path/PathQuery.h"

namespace circuit {

class CQueryPathMulti: public IPathQuery {
public:
	CQueryPathMulti(const CPathFinder& pathfinder, int id);
	virtual ~CQueryPathMulti();

	void InitQuery(const springai::AIFloat3& startPos, float maxRange,
			const F3Vec& targets, float maxThreat, bool endPosOnly);

	void Prepare();

	// Process Data
	PathInfo& GetRefPathInfo() { return *pPath; }
	float& GetRefPathCost() { return pathCost; }

	// Input Data
	springai::AIFloat3& GetStartPos() { return startPos; }
	F3Vec& GetTargets() { return targets; }
	const float GetMaxRange() const { return maxRange; }
	const float GetMaxThreat() const { return maxThreat; }

	// Result
	std::shared_ptr<PathInfo> GetPathInfo() const { return pPath; }
	const float GetPathCost() const { return pathCost; }

private:
	std::shared_ptr<PathInfo> pPath;
	float pathCost = 0.f;

	springai::AIFloat3 startPos;
	F3Vec targets;
	float maxRange = 0.f;
	float maxThreat = 0.f;
	bool endPosOnly = false;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHMULTI_H_
