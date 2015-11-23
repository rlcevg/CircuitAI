/*
 * AttackTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_

#include "task/fighter/FighterTask.h"

namespace circuit {

struct STerrainMapArea;

class CAttackTask: public IFighterTask {
public:
	CAttackTask(ITaskManager* mgr);
	virtual ~CAttackTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	// FIXME: DEBUG
	virtual void OnUnitIdle(CCircuitUnit* unit);
	// FIXME: DEBUG

private:
	void Execute(CCircuitUnit* unit, bool isUpdating);
	void FindTarget(CCircuitUnit* unit, float& minSqDist);

	float lowestRange;
	float highestRange;
	float lowestSpeed;
	float highestSpeed;
	// NOTE: Using unit instead of area directly may save from processing UpdateAreaUsers
	CCircuitUnit* leader;  // slowest, weakest unit

	float minPower;

	bool isRegroup;
	bool isAttack;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_
