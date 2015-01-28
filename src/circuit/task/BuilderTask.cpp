/*
 * BuilderTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/BuilderTask.h"
#include "task/RetreatTask.h"
#include "CircuitAI.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "unit/UnitManager.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "UnitDef.h"
#include "Unit.h"

namespace circuit {

using namespace springai;

CBuilderTask::CBuilderTask(Priority priority,
		UnitDef* buildDef, const AIFloat3& position,
		BuildType type, float cost, int timeout) :
				IConstructTask(priority, buildDef, position, ConstructType::BUILDER),
				buildType(type),
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
	return ((unit->GetCircuitDef()->CanBuild(buildDef) || (target != nullptr)) && (cost > buildPower * MIN_BUILD_SEC));
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

void CBuilderTask::Update(CCircuitAI* circuit)
{

}

void CBuilderTask::OnUnitIdle(CCircuitUnit* unit)
{
	RemoveAssignee(unit);
}

void CBuilderTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	Unit* u = unit->GetUnit();
	if (u->GetHealth() > u->GetMaxHealth() * 0.6) {
		return;
	}

	IUnitManager* manager = unit->GetManager();
	manager->OnUnitDamaged(unit);
	if (target == nullptr) {
		manager->AbortTask(this, unit);
	} else {
		RemoveAssignee(unit);
	}

	unit->GetManager()->GetRetreatTask()->AssignTo(unit);
}

void CBuilderTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	unit->GetManager()->AbortTask(this, unit);
}

CBuilderTask::BuildType CBuilderTask::GetBuildType()
{
	return buildType;
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
	return (buildType < CBuilderTask::BuildType::EXPAND);
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
