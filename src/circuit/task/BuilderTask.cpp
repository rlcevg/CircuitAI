/*
 * BuilderTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/BuilderTask.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "UnitDef.h"

namespace circuit {

using namespace springai;

CBuilderTask::CBuilderTask(Priority priority,
		UnitDef* buildDef, const AIFloat3& position,
		TaskType type, float cost, int timeout) :
				IConstructTask(priority, buildDef, position, ConstructType::BUILDER),
				type(type),
				cost(cost),
				timeout(timeout),
				target(nullptr),
				buildPos(-RgtVector),
				buildPower(.0f),
				facing(UNIT_COMMAND_BUILD_NO_FACING)
{

}

CBuilderTask::~CBuilderTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBuilderTask::CanAssignTo(CCircuitUnit* unit)
{
	return ((unit->GetCircuitDef()->CanBuild(buildDef) && (cost > buildPower * MIN_BUILD_SEC)) || (target != nullptr));
}

void CBuilderTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);
	buildPower += unit->GetDef()->GetBuildSpeed();
}

void CBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);
	buildPower -= unit->GetDef()->GetBuildSpeed();
}

CBuilderTask::TaskType CBuilderTask::GetType()
{
	return type;
}

float CBuilderTask::GetBuildPower()
{
	return buildPower;
}

float CBuilderTask::GetCost()
{
	return cost;
}

int CBuilderTask::GetTimeout()
{
	return timeout;
}

void CBuilderTask::SetBuildPos(const AIFloat3& pos)
{
	buildPos = pos;
}

const AIFloat3& CBuilderTask::GetBuildPos() const
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

bool CBuilderTask::IsStructure()
{
	return (type < CBuilderTask::TaskType::EXPAND);
}

void CBuilderTask::SetFacing(int value)
{
	facing = value;
}

int CBuilderTask::GetFacing()
{
	return facing;
}

} // namespace circuit
