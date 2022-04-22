/*
 * QueryPathWide.h
 *
 *  Created on: Jan 18, 2022
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHWIDE_H_
#define SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHWIDE_H_

#include "terrain/path/PathQuery.h"

namespace circuit {

class CQueryPathWide: public IPathQuery {
public:
	CQueryPathWide(const CPathFinder& pathfinder, int id);
	virtual ~CQueryPathWide();

	void InitQuery(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos, const IndexVec& targets);

	void Prepare();

	// Process Data
	springai::AIFloat3& GetStartPosRef() { return startPos; }
	springai::AIFloat3& GetEndPosRef() { return endPos; }
	IndexVec& GetTargetsRef() { return targets; }
	CPathInfo& GetPathInfoRef() { return *pPath; }
	float& GetPathCostRef() { return pathCost; }

	// Input Data
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	const springai::AIFloat3& GetEndPos() const { return endPos; }

	// Result
	const std::shared_ptr<CPathInfo>& GetPathInfo() const { return pPath; }
	const float GetPathCost() const { return pathCost; }

private:
	std::shared_ptr<CPathInfo> pPath;
	float pathCost = 0.f;

	springai::AIFloat3 startPos;
	springai::AIFloat3 endPos;
	IndexVec targets;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHWIDE_H_
