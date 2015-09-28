/*
 * ScoutTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_

#include "task/fighter/FighterTask.h"
#include "util/Defines.h"

namespace circuit {

class CScoutTask: public IFighterTask {
public:
	CScoutTask(ITaskManager* mgr);
	virtual ~CScoutTask();

	virtual void AssignTo(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

private:
	bool isUpdating;
	unsigned int updCount;
	int scoutIndex;

	CEnemyUnit* FindBestTarget(CCircuitUnit* unit, F3Vec& path);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_SCOUTTASK_H_
