/*
 * AntiAirTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/AntiAirTask.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

CAntiAirTask::CAntiAirTask(ITaskManager* mgr, float enemyAir)
		: IFighterTask(mgr, FightType::AA)
		, enemyAir(enemyAir)
{
}

CAntiAirTask::~CAntiAirTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CAntiAirTask::CanAssignTo(CCircuitUnit* unit) const
{
	return (attackPower < enemyAir) && unit->GetCircuitDef()->IsRoleAA();
}

} // namespace circuit
