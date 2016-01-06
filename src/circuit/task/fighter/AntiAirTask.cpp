/*
 * AntiAirTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/AntiAirTask.h"

namespace circuit {

CAntiAirTask::CAntiAirTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::AA)
{
	// TODO Auto-generated constructor stub

}

CAntiAirTask::~CAntiAirTask()
{
	// TODO Auto-generated destructor stub
}

} // namespace circuit
