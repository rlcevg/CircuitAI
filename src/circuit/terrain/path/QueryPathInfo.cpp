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
		float maxRange, float maxThreat)
{
	this->startPos = startPos;
	this->endPos = endPos;
	this->maxRange = maxRange;
	this->maxThreat = maxThreat;
}

void CQueryPathInfo::Prepare()
{
	pPath = std::make_shared<PathInfo>();
}

} // namespace circuit
