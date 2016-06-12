/*
 * AttackTask.h
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_

#include "task/fighter/SquadTask.h"

#include <memory>

namespace circuit {

struct STerrainMapArea;

class CAttackTask: public ISquadTask {
public:
	CAttackTask(ITaskManager* mgr);
	virtual ~CAttackTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);

private:
	virtual void Merge(const std::set<CCircuitUnit*>& rookies, float power);
	void FindTarget();

	std::shared_ptr<F3Vec> pPath;
	float minPower;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_ATTACKTASK_H_
