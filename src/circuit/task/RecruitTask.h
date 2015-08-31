/*
 * RecruitTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_RECRUITTASK_H_
#define SRC_CIRCUIT_TASK_RECRUITTASK_H_

#include "task/UnitTask.h"

namespace circuit {

class CCircuitDef;

class CRecruitTask: public IUnitTask {
public:
	enum class BuildType: char {BUILDPOWER = 0, FIREPOWER, AA, CLOAK, DEFAULT = FIREPOWER};

public:
	CRecruitTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float radius);
	virtual ~CRecruitTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);

	const springai::AIFloat3& GetTaskPos() const;
	CCircuitDef* GetBuildDef() const;

	BuildType GetBuildType() const;

	void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget() const;

private:
	springai::AIFloat3 position;
	CCircuitDef* buildDef;

	BuildType buildType;
	float sqradius;
	CCircuitUnit* target;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_RECRUITTASK_H_
