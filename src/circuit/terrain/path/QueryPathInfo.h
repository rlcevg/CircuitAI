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
			int radius, float maxThreat);

	const springai::AIFloat3& GetStartPos() const { return startPos; }
	const springai::AIFloat3& GetEndPos() const { return endPos; }
	const int GetRadius() const { return radius; }
	const float GetMaxThreat() const { return maxThreat; }

	const PathInfo& GetPathInfo() const { return iPath; }

private:
	PathInfo iPath;  // result

	springai::AIFloat3 startPos;
	springai::AIFloat3 endPos;
	int radius = 0;
	float maxThreat = 0.f;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATH_QUERYPATHINFO_H_
