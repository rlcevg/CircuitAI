/*
 * QueryCostMap.h
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_QUERYCOSTMAP_H_
#define SRC_CIRCUIT_TERRAIN_PATH_QUERYCOSTMAP_H_

#include "terrain/path/PathQuery.h"

namespace circuit {

class CQueryCostMap: public IPathQuery {
public:
	CQueryCostMap(const CPathFinder& pathfinder, int id);
	virtual ~CQueryCostMap();

	void InitQuery(const springai::AIFloat3& startPos);

	std::vector<float>& PrepareCostMap();
	const springai::AIFloat3& GetStartPos() const { return startPos; }

	float GetCostAt(const springai::AIFloat3& endPos, int radius) const;

private:
	std::vector<float> costMap;  // result

	springai::AIFloat3 startPos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYCOSTMAP_H_
