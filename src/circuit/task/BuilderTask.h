/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef BUILDERTASK_H_
#define BUILDERTASK_H_

#include "task/UnitTask.h"

#define MIN_BUILD_SEC	10
#define MAX_TRAVEL_SEC	60

namespace springai {
	class UnitDef;
}

namespace circuit {

class CBuilderTask: public IUnitTask {
public:
	enum class BuildType: int {
		FACTORY = 0, NANO,
		STORE, PYLON,
		SOLAR, FUSION, SINGU,
		DEFENDER, LOTUS, DDM, ANNI, RAVE,
		RADAR, EXPAND,
		TERRAFORM, ASSIST, RECLAIM, PATROL, TASKS_COUNT, DEFAULT = DEFENDER
	};

public:
	CBuilderTask(Priority priority,
				 springai::UnitDef* buildDef, const springai::AIFloat3& position,
				 BuildType type, float cost, int timeout = 0);
	virtual ~CBuilderTask();

	virtual bool CanAssignTo(CCircuitUnit* unit);
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Update(CCircuitAI* circuit);

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
	void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget();

	bool IsStructure();
	void SetFacing(int value);
	int GetFacing();

private:
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

#endif // BUILDERTASK_H_
