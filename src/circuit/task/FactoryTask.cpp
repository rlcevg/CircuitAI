/*
 * FactoryTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "task/FactoryTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "util/utils.h"

#include "Unit.h"

namespace circuit {

using namespace springai;

CFactoryTask::CFactoryTask(Priority priority,
		UnitDef* buildDef, const AIFloat3& position,
		FacType type, int quantity, float radius) :
				IUnitTask(priority, Type::FACTORY),
				buildDef(buildDef),
				position(position),
				facType(type),
				quantity(quantity),
				sqradius(radius * radius)
{
}

CFactoryTask::~CFactoryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CFactoryTask::CanAssignTo(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();
	return position.SqDistance2D(pos) <= sqradius;
}

void CFactoryTask::Update(CCircuitAI* circuit)
{
	// TODO: Analyze nearby situation, enemies
}

void CFactoryTask::OnUnitIdle(CCircuitUnit* unit)
{
	unit->GetManager()->ExecuteTask(unit);
}

void CFactoryTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	// TODO: React: analyze, abort, create appropriate task
}

void CFactoryTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	RemoveAssignee(unit);
}

const AIFloat3& CFactoryTask::GetPos() const
{
	return position;
}

UnitDef* CFactoryTask::GetBuildDef()
{
	return buildDef;
}

CFactoryTask::FacType CFactoryTask::GetFacType()
{
	return facType;
}

void CFactoryTask::Progress()
{
	quantity--;
}

void CFactoryTask::Regress()
{
	quantity++;
}

bool CFactoryTask::IsDone()
{
	return quantity <= 0;
}

} // namespace circuit
