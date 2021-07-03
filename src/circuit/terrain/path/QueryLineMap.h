/*
 * QuerylineMap.h
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_QUERYLINEMAP_H_
#define SRC_CIRCUIT_TERRAIN_PATH_QUERYLINEMAP_H_

#include "terrain/path/PathQuery.h"

namespace circuit {

class CQueryLineMap: public IPathQuery {
public:
	CQueryLineMap(const CPathFinder& pathfinder, int id);
	virtual ~CQueryLineMap();

	void InitQuery(int threatXSize, int squareSize);

	// Result
	bool IsSafeLine(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos) const;

private:
	int threatXSize = 0;
	int squareSize = 1;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYLINEMAP_H_
