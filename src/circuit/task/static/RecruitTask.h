/*
 * RecruitTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_RECRUITTASK_H_
#define SRC_CIRCUIT_TASK_RECRUITTASK_H_

#include "task/builder/BuilderTask.h"

namespace circuit {

class CCircuitDef;

class CRecruitTask: public IBuilderTask {
public:
	enum class RecruitType: char {BUILDPOWER = 0, FIREPOWER, AA, ARTY, CLOAK, DEFAULT = FIREPOWER};

public:
	CRecruitTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 RecruitType type, float radius);
	virtual ~CRecruitTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
protected:
	virtual void Finish();
	virtual void Cancel();

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);

	RecruitType GetRecruitType() const { return recruitType; }

private:
	RecruitType recruitType;
	float sqradius;
	bool isWait;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RECRUITTASK_H_
