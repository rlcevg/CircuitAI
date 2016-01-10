/*
 * RaidTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/RaidTask.h"

namespace circuit {

CRaidTask::CRaidTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::RAID)
{
	// TODO Auto-generated constructor stub

}

CRaidTask::~CRaidTask()
{
	// TODO Auto-generated destructor stub
}

bool CRaidTask::CanAssignTo(CCircuitUnit* unit)
{
	return units.empty();
}

} // namespace circuit
