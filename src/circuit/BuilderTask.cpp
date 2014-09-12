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

CBuilderTask::CBuilderTask(Priority priority, int difficulty, TaskType type, springai::AIFloat3& position, float radius) :
		IUnitTask(priority, difficulty),
		type(type),
		position(position),
		sqradius(radius * radius)
{
}

CBuilderTask::~CBuilderTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBuilderTask::AssignTo(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	AIFloat3 pos = u->GetPos();
	if (!IsDistanceOk(pos)) {
		return false;
	}

	difficulty--;
	unit->SetTask(this);
	units.insert(unit);

	return true;
}

void CBuilderTask::MarkCompleted()
{
	for (auto& unit : units) {
		unit->SetTask(nullptr);
	}
	units.clear();
}

CBuilderTask::TaskType CBuilderTask::GetType()
{
	return type;
}

bool CBuilderTask::IsDistanceOk(AIFloat3& pos)
{
	float dx = pos.x - position.x;
	float dz = pos.z - position.z;
	float sqdistance = dx * dx + dz * dz;
	return sqdistance <= sqradius;
}

} // namespace circuit
