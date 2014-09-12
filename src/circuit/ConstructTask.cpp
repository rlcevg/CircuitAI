/*
 * ConstructTask.cpp
 *
 *  Created on: Sep 12, 2014
 *      Author: rlcevg
 */

#include "ConstructTask.h"
#include "CircuitUnit.h"

#include "Unit.h"

namespace circuit {

using namespace springai;

IConstructTask::IConstructTask(Priority priority, int difficulty, AIFloat3& position, float radius, float metal, std::list<IConstructTask*>* owner) :
		IUnitTask(priority, difficulty),
		position(position),
		sqradius(radius * radius),
		metalToSpend(metal),
		owner(owner)
{
}

IConstructTask::~IConstructTask()
{
}

bool IConstructTask::CanAssignTo(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();
	AIFloat3 pos = u->GetPos();
	return IsDistanceOk(pos);
}

bool IConstructTask::CompleteProgress(float metalStep)
{
	metalToSpend -= metalStep;
	if (metalToSpend <= 0.0f) {
		MarkCompleted();
		owner->remove(this);
		return true;
	}
	return false;
}

float IConstructTask::GetMetalToSpend()
{
	return metalToSpend;
}

bool IConstructTask::IsDistanceOk(AIFloat3& pos)
{
	float dx = pos.x - position.x;
	float dz = pos.z - position.z;
	float sqdistance = dx * dx + dz * dz;
	return sqdistance <= sqradius;
}

} // namespace circuit
