/*
 * FactoryTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "FactoryTask.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "Unit.h"

namespace circuit {

using namespace springai;

CFactoryTask::CFactoryTask(Priority priority, int quantity, AIFloat3& position, std::list<IConstructTask*>& owner, TaskType type, float radius) :
		IConstructTask(priority, quantity, position, owner),
		type(type),
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
	AIFloat3 pos = u->GetPos();
	return IsDistanceOk(pos);
}

CFactoryTask::TaskType CFactoryTask::GetType()
{
	return type;
}

bool CFactoryTask::IsDistanceOk(AIFloat3& pos)
{
	float dx = pos.x - position.x;
	float dz = pos.z - position.z;
	float sqdistance = dx * dx + dz * dz;
	return sqdistance <= sqradius;
}

} // namespace circuit
