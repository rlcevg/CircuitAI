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

CBuilderTask::CBuilderTask(Priority priority, int quantity, springai::AIFloat3& position, std::list<IConstructTask*>& owner, TaskType type, float time) :
		IConstructTask(priority, quantity, position, owner),
		type(type),
		time(time)
{
}

CBuilderTask::~CBuilderTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBuilderTask::CanAssignTo(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	AIFloat3 pos = u->GetPos();
	float speed = u->GetMaxSpeed();
	return IsDistanceOk(pos, speed);
}

CBuilderTask::TaskType CBuilderTask::GetType()
{
	return type;
}

bool CBuilderTask::IsDistanceOk(AIFloat3& pos, float speed)
{
	float dx = pos.x - position.x;
	float dz = pos.z - position.z;
	float distance = math::sqrt(dx * dx + dz * dz);
	return distance / (speed * FRAMES_PER_SEC) <= time;
}

} // namespace circuit
