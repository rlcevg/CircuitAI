/*
 * QueryPathMulti.cpp
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#include "terrain/path/QueryPathMulti.h"

namespace circuit {

using namespace springai;

CQueryPathMulti::CQueryPathMulti(const CPathFinder& pathfinder, int id)
		: IPathQuery(pathfinder, id)
{
}

CQueryPathMulti::~CQueryPathMulti()
{
}

void CQueryPathMulti::InitQuery(const AIFloat3& startPos, float maxRange,
		F3Vec targets, float maxThreat)
{
	this->startPos = startPos;
	this->maxRange = maxRange;
	this->targets = targets;
	this->maxThreat = maxThreat;
}

void CQueryPathMulti::Prepare()
{
	pPath = std::make_shared<PathInfo>();
}

} // namespace circuit
