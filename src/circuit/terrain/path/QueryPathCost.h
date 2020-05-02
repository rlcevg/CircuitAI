/*
 * QueryPathCost.h
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHCOST_H_
#define SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHCOST_H_

#include "terrain/path/PathQuery.h"

namespace circuit {

class CQueryPathCost: public IPathQuery {
public:
	CQueryPathCost(const CPathFinder& pathfinder, int id);
	virtual ~CQueryPathCost();

	void InitQuery(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos,
			int radius, float maxThreat);

	// Process Data
	float& GetRefPathCost() { return pathCost; }

	// Input Data
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	const springai::AIFloat3& GetEndPos() const { return endPos; }
	const int GetRadius() const { return radius; }
	const float GetMaxThreat() const { return maxThreat; }

	// Result
	const float GetPathCost() const { return pathCost; }

private:
	float pathCost = 0.f;

	springai::AIFloat3 startPos;
	springai::AIFloat3 endPos;
	int radius = 0;
	float maxThreat = 0.f;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHCOST_H_
