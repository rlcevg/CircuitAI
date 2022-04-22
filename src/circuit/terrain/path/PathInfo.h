/*
 * PathInfo.h
 *
 *  Created on: Apr 21, 2022
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATH_PATHINFO_H_
#define SRC_CIRCUIT_TERRAIN_PATH_PATHINFO_H_

#include "util/Defines.h"

namespace circuit {

class CPathFinder;

class CPathInfo {
public:
	CPathInfo(bool last = false) : start(0), isEndPos(last) {}
	~CPathInfo() {}

	void Clear() { posPath.clear(); path.clear(); }  // FIXME: stop TravelAction
	void PushPos(const springai::AIFloat3& pos, CPathFinder* pathfinder);

	F3Vec posPath;
	IndexVec path;
	size_t start;
	bool isEndPos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_PATHINFO_H_
