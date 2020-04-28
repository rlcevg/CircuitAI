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
			F3Vec targets, float maxThreat);

	// Input Data
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	const float GetMaxRange() const { return maxRange; }
	const F3Vec& GetTargets() const { return targets; }
	const float GetMaxThreat() const { return maxThreat; }

	// Result
	const PathInfo& GetPathInfo() const { return iPath; }
	const float GetPathCost() const { return pathCost; }

private:
	PathInfo iPath;
	float pathCost = 0.f;

	springai::AIFloat3 startPos;
	float maxRange = 0.f;
	F3Vec targets;
	float maxThreat = 0.f;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHMULTI_H_
