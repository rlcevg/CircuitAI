/*
 * QueryPathCost.cpp
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#include "terrain/path/QueryPathCost.h"

namespace circuit {

using namespace springai;

CQueryPathCost::CQueryPathCost(const CPathFinder& pathfinder, int id)
		: IPathQuery(pathfinder, id)
{
}

CQueryPathCost::~CQueryPathCost()
{
}

void CQueryPathCost::InitQuery(const AIFloat3& startPos, const AIFloat3& endPos,
		int radius, float maxThreat)
{
	this->startPos = startPos;
	this->endPos = endPos;
	this->radius = radius;
	this->maxThreat = maxThreat;
}

} // namespace circuit
