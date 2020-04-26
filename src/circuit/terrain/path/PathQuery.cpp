/*
 * PathQuery.cpp
 *
 *  Created on: Apr 22, 2020
 *      Author: rlcevg
 */

#include "terrain/path/PathQuery.h"

namespace circuit {

IPathQuery::IPathQuery(const CPathFinder& pathfinder, int id)
		: pathfinder(pathfinder)
		, id(id)
		, type(Type::NONE)
		, canMoveArray(nullptr)
		, threatArray(nullptr)
{
}

IPathQuery::~IPathQuery()
{
}

void IPathQuery::Init(const bool* canMoveArray, const float* threatArray,
		NSMicroPather::CostFunc moveThreatFun, NSMicroPather::CostFunc moveFun)
{
	this->canMoveArray = canMoveArray;
	this->threatArray = threatArray;
	this->moveThreatFun = moveThreatFun;
	this->moveFun = moveFun;
}

} // namespace circuit
