/*
 * BuilderyTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "BuilderTask.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "Unit.h"

namespace circuit {

using namespace springai;

CBuilderTask::CBuilderTask(Priority priority,
		const AIFloat3& position,
		TaskType type, int timeout) :
				IConstructTask(priority, position, ConstructType::BUILDER),
				type(type),
				timeout(timeout),
				quantity(1),
				target(nullptr),
				buildPos(-RgtVector)
{
}

CBuilderTask::~CBuilderTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBuilderTask::CanAssignTo(CCircuitUnit* unit)
{
	return true;
}

void CBuilderTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);
	quantity++;
}

void CBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);
	quantity--;
}

CBuilderTask::TaskType CBuilderTask::GetType()
{
	return type;
}

int CBuilderTask::GetQuantity()
{
	return quantity;
}

int CBuilderTask::GetTimeout()
{
	return timeout;
}

void CBuilderTask::SetBuildPos(const AIFloat3& pos)
{
	buildPos = pos;
}

AIFloat3& CBuilderTask::GetBuildPos()
{
	return buildPos;
}

void CBuilderTask::SetTarget(CCircuitUnit* unit)
{
	target = unit;
}

CCircuitUnit* CBuilderTask::GetTarget()
{
	return target;
}

} // namespace circuit
