/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_

#include "task/UnitTask.h"

#define MIN_BUILD_SEC	10
#define MAX_BUILD_SEC	120
#define MAX_TRAVEL_SEC	60

namespace springai {
	class UnitDef;
}

namespace circuit {

class IBuilderTask: public IUnitTask {
public:
	enum class BuildType: int {
		FACTORY = 0,
		NANO,
		STORE,
		PYLON,
		ENERGY,
		DEFENCE,  // lotus, defender
		BUNKER,  // stardust, stinger, ddm, anni
		BIG_GUN,  // super weapons
		RADAR,
		MEX,
		TERRAFORM, REPAIR, RECLAIM, PATROL,  // Other builder actions
		TASKS_COUNT, DEFAULT = BIG_GUN
	};

protected:
	IBuilderTask(CCircuitAI* circuit, Priority priority,
				 springai::UnitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float cost, int timeout = 0);
public:
	virtual ~IBuilderTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();

	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	const springai::AIFloat3& GetPos() const;
	springai::UnitDef* GetBuildDef();

	BuildType GetBuildType();
	float GetBuildPower();
	float GetCost();
	int GetTimeout();

	void SetBuildPos(const springai::AIFloat3& pos);
	const springai::AIFloat3& GetBuildPos() const;
	virtual void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget();

	bool IsStructure();
	void SetFacing(int value);
	int GetFacing();

protected:
	int FindFacing(springai::UnitDef* buildDef, const springai::AIFloat3& position);

	springai::AIFloat3 position;
	springai::UnitDef* buildDef;

	BuildType buildType;
	float buildPower;
	float cost;
	int timeout;  // TODO: re-evaluate need of this
	CCircuitUnit* target;
	springai::AIFloat3 buildPos;
	int facing;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
