/*
 * AntiAirTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/AntiAirTask.h"

namespace circuit {

CAntiAirTask::CAntiAirTask(ITaskManager* mgr, float enemyAir)
		: IFighterTask(mgr, FightType::AA)
		, enemyAir(enemyAir)
{
}

CAntiAirTask::~CAntiAirTask()
{
}

bool CAntiAirTask::CanAssignTo(CCircuitUnit* unit)
{
	return attackPower < enemyAir;
}

} // namespace circuit
