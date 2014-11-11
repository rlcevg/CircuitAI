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

void CBuilderTask::AssignTo(CCircuitUnit* unit, CCircuitAI* circuit)
{
	IUnitTask::AssignTo(unit, circuit);
	quantity++;
	circuit->LOG("Assign: %lu %i | Pos: x(%.2f) z(%.2f) | BuildPos: x(%.2f) z(%.2f) | To:", this, type, position.x, position.z, buildPos.x, buildPos.z);
	for (auto unit : units) {
		circuit->LOG("assignee: %i", unit->GetUnit()->GetUnitId());
	}
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

} // namespace circuit
