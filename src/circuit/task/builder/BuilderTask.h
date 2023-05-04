/*
 * BuilderTask.h
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
#define SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_

#include "task/UnitTask.h"
#include "util/Defines.h"
#include "util/math/Geometry.h"

#include <map>
#include <vector>

namespace circuit {

class CCircuitDef;
class CAllyUnit;

struct SBuildChain;

struct SResource {
	float metal;
	float energy;
};

class IBuilderTask: public IUnitTask {
public:
	enum class BuildType: char {
		FACTORY = 0,
		NANO,
		STORE,
		PYLON,
		ENERGY,
		GEO,
		GEOUP,
		DEFENCE,  // lotus, defender, stardust, stinger
		BUNKER,  // ddm, anni
		BIG_GUN,  // super weapons
		RADAR,
		SONAR,
		CONVERT,
		MEX,
		MEXUP,
		REPAIR,
		RECLAIM,
		RESURRECT,
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
				 Type type, BuildType buildType, SResource cost, float shake = SQUARE_SIZE * 32, int timeout = ASSIGN_TIMEOUT);
	IBuilderTask(ITaskManager* mgr, Type type, BuildType buildType);  // Load
public:
	virtual ~IBuilderTask();

	virtual bool CanAssignTo(CCircuitUnit* unit) const override;
	virtual void AssignTo(CCircuitUnit* unit) override;
	virtual void RemoveAssignee(CCircuitUnit* unit) override;

	virtual void Start(CCircuitUnit* unit) override;
	virtual void Update() override;
	virtual void Stop(bool done) override;
protected:
	virtual void Finish() override;
	virtual void Cancel() override;

	virtual bool Execute(CCircuitUnit* unit);

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) override;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	virtual void OnTravelEnd(CCircuitUnit* unit) override;

	virtual void Activate();
	void Deactivate();

	const springai::AIFloat3& GetTaskPos() const { return position; }
	CCircuitDef* GetBuildDef() const { return buildDef; }

	virtual bool IsGeneric() const { return false; }
	BuildType GetBuildType() const { return buildType; }
	float GetBuildPowerM() const { return buildPower.metal; }
	float GetBuildPowerE() const { return buildPower.energy; }
	float GetCostM() const { return cost.metal; }
	float GetCostE() const { return cost.energy; }

	virtual void SetBuildPos(const springai::AIFloat3& pos);
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

	float ClampPriority() const { return std::min(static_cast<float>(priority), 1.0f); }  // FIXME: BA

protected:
	CCircuitUnit* GetNextAssignee();
	void Update(CCircuitUnit* unit);
	virtual bool Reevaluate(CCircuitUnit* unit);
	void UpdatePath(CCircuitUnit* unit);
	void ApplyPath(const CQueryPathSingle* query);
	void HideAssignee(CCircuitUnit* unit);
	void ShowAssignee(CCircuitUnit* unit);
	virtual CAllyUnit* FindSameAlly(CCircuitUnit* builder, const std::vector<springai::Unit*>& friendlies);
	virtual void FindBuildSite(CCircuitUnit* builder, const springai::AIFloat3& pos, float searchRadius);
	void FindFacing(const springai::AIFloat3& pos);

	void ExecuteChain(SBuildChain* chain);

	virtual bool Load(std::istream& is) override;
	virtual void Save(std::ostream& os) const override;

	BuildType buildType;
	springai::AIFloat3 position;
	float shake;  // Alter/randomize position by offset
	CCircuitDef* buildDef;

	SResource buildPower;  // task's metal and energy per second expenditure
	SResource cost;
	CCircuitUnit* target;  // FIXME: Replace target with unitId
	springai::AIFloat3 buildPos;
	int facing;
	IBuilderTask* nextTask;  // old list style
	CCircuitUnit* initiator;

	SResource savedIncome;
	int buildFails;

	decltype(units)::const_iterator unitIt;  // update iterator

	std::set<CCircuitUnit*> traveled;
	std::set<CCircuitUnit*> executors;

#ifdef DEBUG_VIS
	virtual void Log() override;
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_BUILDER_BUILDERTASK_H_
