/*
 * QueryPathSingle.cpp
 *
 *  Created on: Apr 26, 2020
 *      Author: rlcevg
 */

#include "terrain/path/QueryPathSingle.h"

namespace circuit {

using namespace springai;

CQueryPathSingle::CQueryPathSingle(const CPathFinder& pathfinder, int id)
		: IPathQuery(pathfinder, id, Type::SINGLE)
{
}

CQueryPathSingle::~CQueryPathSingle()
{
}

void CQueryPathSingle::InitQuery(const AIFloat3& startPos, const AIFloat3& endPos,
		float maxRange, NSMicroPather::HitFunc&& hitTest,
		float maxThreat, bool endPosOnly)
{
	this->startPos = startPos;
	this->endPos = endPos;
	this->maxRange = maxRange;
	this->hitTest = hitTest;
	this->maxThreat = maxThreat;
	this->endPosOnly = endPosOnly;
}

void CQueryPathSingle::Prepare()
{
	pPath = std::make_shared<CPathInfo>(endPosOnly);
	if (hitTest == nullptr) {
		hitTest = [](int2 start, int2 end) {
			return true;
		};
	}
}

} // namespace circuit
