/*
 * DefendTask.h
 *
 *  Created on: Feb 12, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_

#include "task/fighter/SquadTask.h"

namespace circuit {

class CDefendTask: public ISquadTask {
public:
	CDefendTask(ITaskManager* mgr, const springai::AIFloat3& position, float radius,
				FightType check, FightType promote, float maxPower);
	virtual ~CDefendTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	void SetPosition(const springai::AIFloat3& pos) { position = pos; }
	void SetWantedTarget(CEnemyUnit* enemy) { SetTarget(enemy); }

protected:
	FightType GetPromote() const { return promote; }
	float GetMaxPower() const { return maxPower; }
	float GetPower() const { return power; }

private:
	virtual void Merge(ISquadTask* task);
	void FindTarget();

	float radius;

	FightType check;
	FightType promote;
	float maxPower;
	float power;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_DEFENDTASK_H_
