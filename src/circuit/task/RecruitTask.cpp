/*
 * RecruitTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/RecruitTask.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CRecruitTask::CRecruitTask(ITaskManager* mgr, Priority priority,
		CCircuitDef* buildDef, const AIFloat3& position,
		BuildType type, float radius)
				: IUnitTask(mgr, priority, Type::FACTORY)
				, buildDef(buildDef)
				, position(position)
				, buildType(type)
				, sqradius(radius * radius)
				, target(nullptr)
{
}

CRecruitTask::~CRecruitTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CRecruitTask::CanAssignTo(CCircuitUnit* unit)
{
	return (target == nullptr) && unit->GetCircuitDef()->CanBuild(buildDef) &&
			position.SqDistance2D(unit->GetUnit()->GetPos()) <= sqradius;
}

void CRecruitTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	const AIFloat3& buildPos = u->GetPos();
	u->Build(buildDef->GetUnitDef(), buildPos, UNIT_COMMAND_BUILD_NO_FACING);
}

void CRecruitTask::Update()
{
	// TODO: Analyze nearby situation, enemies
}

void CRecruitTask::OnUnitIdle(CCircuitUnit* unit)
{
	Execute(unit);
}

void CRecruitTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	// TODO: React: analyze, abort, create appropriate task
}

void CRecruitTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
