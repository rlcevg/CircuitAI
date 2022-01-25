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

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual void Execute(CCircuitUnit* unit) override;

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	virtual void SetTarget(CCircuitUnit* unit) override;

	RecruitType GetRecruitType() const { return recruitType; }

private:
	RecruitType recruitType;
	float sqradius;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RECRUITTASK_H_
