/*
 * RetreatTask.h
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_RETREATTASK_H_
#define SRC_CIRCUIT_TASK_RETREATTASK_H_

#include "task/UnitTask.h"
#include "util/Defines.h"

namespace circuit {

class CQueryCostMap;

class CRetreatTask: public IUnitTask {
public:
	CRetreatTask(ITaskManager* mgr, int timeout = ASSIGN_TIMEOUT);
	virtual ~CRetreatTask();

	virtual void ClearRelease() override;

	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	void CheckRepairer(CCircuitUnit* newRep);
	void SetRepairer(CCircuitUnit* unit) { repairer = unit; }
	CCircuitUnit* GetRepairer() const { return repairer; }

private:
	void ApplyPath(std::shared_ptr<CQueryPathSingle> query);
	CCircuitUnit* ValidateNewRepairer(std::shared_ptr<IPathQuery> query, int newRepId);
	void ApplyCostMap(std::shared_ptr<CQueryCostMap> query, CCircuitUnit* newRep);

	CCircuitUnit* repairer;
	std::map<CCircuitUnit*, std::shared_ptr<IPathQuery>> costQueries;  // IPathQuery owner
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RETREATTASK_H_
