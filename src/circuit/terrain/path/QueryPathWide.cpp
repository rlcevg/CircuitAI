/*
 * QueryPathWide.cpp
 *
 *  Created on: Jan 18, 2022
 *      Author: rlcevg
 */

#include "terrain/path/QueryPathWide.h"

namespace circuit {

using namespace springai;

CQueryPathWide::CQueryPathWide(const CPathFinder& pathfinder, int id)
		: IPathQuery(pathfinder, id, Type::WIDE)
{
}

CQueryPathWide::~CQueryPathWide()
{
}

void CQueryPathWide::InitQuery(const AIFloat3& startPos, const AIFloat3& endPos, const IndexVec& targets)
{
	this->startPos = startPos;
	this->endPos = endPos;
	this->targets = targets;
}

void CQueryPathWide::Prepare()
{
	pPath = std::make_shared<PathInfo>(false);
}

} // namespace circuit
