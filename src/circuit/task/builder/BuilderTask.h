/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_

#include "task/UnitTask.h"
#include "util/utils.h"

#include <map>

namespace circuit {

class CCircuitDef;

struct SBuildChain;

class IBuilderTask: public IUnitTask {
public:
	enum class BuildType: char {
		FACTORY = 0,
		NANO,
		STORE,
		PYLON,
		ENERGY,
		DEFENCE,  // lotus, defender, stardust, stinger
		BUNKER,  // ddm, anni
		BIG_GUN,  // super weapons
		RADAR,
		SONAR,
		MEX,
		REPAIR,
		RECLAIM,
		TERRAFORM,
		_SIZE_,  // selectable tasks count
		RECRUIT,  // builder actions that can't be reassigned
		PATROL, GUARD,
		DEFAULT = BIG_GUN
	};
	using BT = std::underlying_type<BuildType>::type;

	using BuildName = std::map<std::string, BuildType>;
	static BuildName& GetBuildNames() { return buildNames; }
private:
	static BuildName buildNames;

protected:
	IBuilderTask(ITaskManager* mgr, Priority priority,
				 CCircuitDef* buildDef, const springai::AIFloat3& position,
				 Type type, BuildType buildType, float cost, float shake = SQUARE_SIZE * 32, int timeout = ASSIGN_TIMEOUT);
public:
	virtual ~IBuilderTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Execute(CCircuitUnit* unit);
	virtual void Update();
	virtual void Close(bool done);
protected:
	virtual void Finish();
	virtual void Cancel();

public:
	virtual void OnUnitIdle(CCircuitUnit* unit);
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);

	void Activate();
	void Deactivate();

	const springai::AIFloat3& GetTaskPos() const { return position; }
	CCircuitDef* GetBuildDef() const { return buildDef; }

	BuildType GetBuildType() const { return buildType; }
	float GetBuildPower() const { return buildPower; }
	float GetCost() const { return cost; }

	void SetBuildPos(const springai::AIFloat3& pos) { buildPos = pos; }
	const springai::AIFloat3& GetBuildPos() const { return buildPos; }
	const springai::AIFloat3& GetPosition() const { return utils::is_valid(buildPos) ? buildPos : position; }

	virtual void SetTarget(CCircuitUnit* unit);
	CCircuitUnit* GetTarget() const { return target; }
	void UpdateTarget(CCircuitUnit* unit);

	bool IsEqualBuildPos(CCircuitUnit* unit) const;

	void SetFacing(int value) { facing = value; }
	int GetFacing() const { return facing; }

	void SetNextTask(IBuilderTask* task) { nextTask = task; }
	IBuilderTask* GetNextTask() const { return nextTask; }

	float ClampPriority() const { return std::min(static_cast<float>(priority), 2.0f); }

protected:
	void HideAssignee(CCircuitUnit* unit);
	void ShowAssignee(CCircuitUnit* unit);
	virtual void FindBuildSite(CCircuitUnit* builder, const springai::AIFloat3& pos, float searchRadius);

	void ExecuteChain(SBuildChain* chain);

	virtual void Load(std::istream& is);
	virtual void Save(std::ostream& os) const;

	springai::AIFloat3 position;
	float shake;  // Alter/randomize position by offset
	CCircuitDef* buildDef;

	BuildType buildType;
	float buildPower;
	float cost;
	CCircuitUnit* target;  // FIXME: Replace target with unitId
	springai::AIFloat3 buildPos;
	int facing;
	IBuilderTask* nextTask;

	float savedIncome;
	int buildFails;

	decltype(units)::const_iterator unitIt;  // update iterator
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
