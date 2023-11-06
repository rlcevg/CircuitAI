/*
 * RaidTask.h
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_FIGHTER_RAIDTASK_H_
#define SRC_CIRCUIT_TASK_FIGHTER_RAIDTASK_H_

#include "task/fighter/SquadTask.h"

namespace circuit {

class CRaidTask: public ISquadTask {
public:
	CRaidTask(IUnitModule* mgr, float maxPower, float powerMod);
	virtual ~CRaidTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
	virtual void Stop(bool done);

	virtual void OnUnitIdle(CCircuitUnit* unit) override;

private:
	bool FindTarget();
	void ApplyTargetPath(const CQueryPathMulti* query);
	void FallbackRaid();
	void ApplyRaidPath(const CQueryPathSingle* query);

	float maxPower;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_FIGHTER_RAIDTASK_H_
