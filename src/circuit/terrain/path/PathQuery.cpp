/*
 * PathQuery.cpp
 *
 *  Created on: Apr 22, 2020
 *      Author: rlcevg
 */

#include "terrain/path/PathQuery.h"
//#include "task/UnitTask.h"

namespace circuit {

IPathQuery::IPathQuery(const CPathFinder& pathfinder, int id, Type type)
		: pathfinder(pathfinder)
		, heightMap(pathfinder.GetHeightMap())
		, id(id)
		, type(type)
		, state(State::NONE)
		, canMoveArray(nullptr)
		, threatArray(nullptr)
		, unit(nullptr)
//		, taskHolder(nullptr)
{
}

IPathQuery::~IPathQuery()
{
//	if (taskHolder != nullptr) {
//		taskHolder->Release();
//	}
}

void IPathQuery::Init(const bool* canMoveArray, const float* threatArray,
		NSMicroPather::CostFunc moveFun, NSMicroPather::CostFunc threatFun,
		CCircuitUnit* unit)
{
	this->canMoveArray = canMoveArray;
	this->threatArray = threatArray;
	this->moveFun = moveFun;
	this->threatFun = threatFun;
	this->unit = unit;  // optional
}

//void IPathQuery::HoldTask(IUnitTask* task)
//{
//	taskHolder = task;
//	taskHolder->AddRef();
//}

} // namespace circuit
