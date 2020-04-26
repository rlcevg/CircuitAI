/*
 * QueryPathInfo.cpp
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#include "terrain/path/QueryPathInfo.h"

namespace circuit {

using namespace springai;

CQueryPathInfo::CQueryPathInfo(const CPathFinder& pathfinder, int id)
		: IPathQuery(pathfinder, id)
{
}

CQueryPathInfo::~CQueryPathInfo()
{
}

void CQueryPathInfo::InitQuery(const AIFloat3& startPos, const AIFloat3& endPos,
		int radius, float maxThreat)
{
	this->startPos = startPos;
	this->endPos = endPos;
	this->radius = radius;
	this->maxThreat = maxThreat;
}

} // namespace circuit
