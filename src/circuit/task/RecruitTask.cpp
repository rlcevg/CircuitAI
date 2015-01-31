/*
 * RecruitTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/RecruitTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Unit.h"

namespace circuit {

using namespace springai;

CRecruitTask::CRecruitTask(CCircuitAI* circuit, Priority priority,
		UnitDef* buildDef, const AIFloat3& position,
		FacType type, int quantity, float radius) :
				IUnitTask(circuit, priority, Type::FACTORY),
				buildDef(buildDef),
				position(position),
				facType(type),
				quantity(quantity),
				sqradius(radius * radius)
{
}

CRecruitTask::~CRecruitTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CRecruitTask::CanAssignTo(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();
	return position.SqDistance2D(pos) <= sqradius;
}

void CRecruitTask::Execute(CCircuitUnit* unit)
{
	CRecruitTask* task = static_cast<CRecruitTask*>(unit->GetTask());
	Unit* u = unit->GetUnit();
	const AIFloat3& buildPos = u->GetPos();

	UnitDef* buildDef = task->GetBuildDef();
	u->Build(buildDef, buildPos, UNIT_COMMAND_BUILD_NO_FACING);
}

void CRecruitTask::Update()
{
	// TODO: Analyze nearby situation, enemies
}

void CRecruitTask::OnUnitIdle(CCircuitUnit* unit)
{
	Execute(unit);
}

void CRecruitTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	// TODO: React: analyze, abort, create appropriate task
}

void CRecruitTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	RemoveAssignee(unit);
}

const AIFloat3& CRecruitTask::GetPos() const
{
	return position;
}

UnitDef* CRecruitTask::GetBuildDef()
{
	return buildDef;
}

CRecruitTask::FacType CRecruitTask::GetFacType()
{
	return facType;
}

void CRecruitTask::Progress()
{
	quantity--;
}

void CRecruitTask::Regress()
{
	quantity++;
}

bool CRecruitTask::IsDone()
{
	return quantity <= 0;
}

} // namespace circuit
