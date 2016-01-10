/*
 * ArtilleryTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/ArtilleryTask.h"

namespace circuit {

CArtilleryTask::CArtilleryTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::ARTY)
{
	// TODO Auto-generated constructor stub

}

CArtilleryTask::~CArtilleryTask()
{
	// TODO Auto-generated destructor stub
}

bool CArtilleryTask::CanAssignTo(CCircuitUnit* unit)
{
	return units.empty();
}

} // namespace circuit
