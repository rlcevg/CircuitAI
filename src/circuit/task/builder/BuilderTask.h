/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_

#include "task/UnitTask.h"

#define MIN_BUILD_SEC	20
#define MAX_BUILD_SEC	60
#define MAX_TRAVEL_SEC	180

namespace circuit {

class CCircuitDef;

class IBuilderTask: public IUnitTask {
public:
	enum class BuildType: int {
		FACTORY = 0,
		NANO,
		STORE,
		PYLON,
		ENERGY,
		DEFENCE,  // lotus, defender, stardust, stinger
		BUNKER,  // ddm, anni
		BIG_GUN,  // super weapons
		RADAR,
		MEX,
		TERRAFORM, REPAIR, RECLAIM, PATROL,  // Other builder actions
		TASKS_COUNT, DEFAULT = BIG_GUN
	};

protected:
	IBuilderTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float cost, bool isShake = true, int timeout = 0);
public:
	virtual ~IBuilderTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
protected:
	virtual void Finish();
	virtual void Cancel();

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	const springai::AIFloat3& GetTaskPos() const;
	CCircuitDef* GetBuildDef() const;

	BuildType GetBuildType() const;
	float GetBuildPower() const;
	float GetCost() const;
	int GetTimeout() const;

	void SetBuildPos(const springai::AIFloat3& pos);
	const springai::AIFloat3& GetBuildPos() const;
	const springai::AIFloat3& GetPosition() const;  // return buildPos if set, position otherwise
	virtual void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget() const;

	bool IsStructure() const;
	void SetFacing(int value);
	int GetFacing() const;

protected:
	int FindFacing(CCircuitDef* buildDef, const springai::AIFloat3& position);

	springai::AIFloat3 position;
	bool isShake;  // Alter/randomize position
	CCircuitDef* buildDef;

	BuildType buildType;
	float buildPower;
	float cost;
	int timeout;  // TODO: re-evaluate need of this
	CCircuitUnit* target;  // FIXME: Replace target with unitId
	springai::AIFloat3 buildPos;
	int facing;

	float savedIncome;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
