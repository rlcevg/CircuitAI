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
		: IPathQuery(pathfinder, id, Type::MULTI)
{
}

CQueryPathMulti::~CQueryPathMulti()
{
}

void CQueryPathMulti::InitQuery(const AIFloat3& startPos, float maxRange,
		const F3Vec& targets, float maxThreat, bool endPosOnly)
{
	this->startPos = startPos;
	this->maxRange = maxRange;
	this->targets = targets;
	this->maxThreat = maxThreat;
	this->endPosOnly = endPosOnly;
}

void CQueryPathMulti::Prepare()
{
	pPath = std::make_shared<PathInfo>(endPosOnly);
}

} // namespace circuit
