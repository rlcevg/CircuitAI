/*
 * FactoryManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
#define SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_

#include "module/UnitModule.h"
#include "task/static/RecruitTask.h"
#include "unit/CircuitUnit.h"

#include <map>

namespace circuit {

class CEconomyManager;
class CFactoryData;
class CSRepairTask;

namespace TaskS {
	struct SRecruitTask {
		CRecruitTask::RecruitType type;
		CRecruitTask::Priority priority;
		CCircuitDef* buildDef;
		springai::AIFloat3 position;
		float radius;
	};

	struct SServSTask {
		IBuilderTask::BuildType type;
		IBuilderTask::Priority priority;
		springai::AIFloat3 position;
		CAllyUnit* target;
		float radius;
		bool stop;
		int timeout;
	};

	static inline SRecruitTask Recruit(CRecruitTask::RecruitType type,
			CRecruitTask::Priority priority, CCircuitDef* buildDef,
			const springai::AIFloat3& position, float radius)
	{
		SRecruitTask ti;
		ti.type = type;
		ti.priority = priority;
		ti.buildDef = buildDef;
		ti.position = position;
		ti.radius = radius;
		return ti;
	}

	static inline SServSTask Repair(IBuilderTask::Priority priority, CAllyUnit* target)
	{
		SServSTask ti;
		ti.type = IBuilderTask::BuildType::REPAIR;
		ti.priority = priority;
		ti.target = target;
		return ti;
	}
	static inline SServSTask Reclaim(IBuilderTask::Priority priority,
			const springai::AIFloat3& position, float radius, int timeout = 0)
	{
		SServSTask ti;
		ti.type = IBuilderTask::BuildType::RECLAIM;
		ti.priority = priority;
		ti.position = position;
		ti.radius = radius;
		ti.timeout = timeout;
		return ti;
	}
	static inline SServSTask Wait(bool stop, int timeout)
	{
		SServSTask ti;
		ti.type = IBuilderTask::BuildType::WAIT;
		ti.stop = stop;
		ti.timeout = timeout;
		return ti;
	}
} // namespace TaskS

class CFactoryManager: public IUnitModule {
public:
	friend class CFactoryScript;

	struct SSideInfo {
		CCircuitDef* assistDef;
	};

	CFactoryManager(CCircuitAI* circuit);
	virtual ~CFactoryManager();

private:
	void ReadConfig();
	void Init();

public:
	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	CRecruitTask* Enqueue(const TaskS::SRecruitTask& ti);
	IUnitTask* Enqueue(const TaskS::SServSTask& ti);
private:
	virtual void DequeueTask(IUnitTask* task, bool done = false) override;

public:
	void MarkRepairUnit(ICoreUnit::Id targetId, CSRepairTask* task) {
		repairUnits[targetId] = task;
	}

	int GetFactoryCount() const { return factories.size(); }
	int GetNoT1FacCount() const { return noT1FacCount; }
	float GetMetalRequire() const { return metalRequire; }
	float GetEnergyRequire() const { return energyRequire; }
	float GetNewFacModM() const { return newFacModM; }
	float GetNewFacModE() const { return newFacModE; }
	float GetFacModM() const { return facModM; }
	float GetFacModE() const { return facModE; }
	bool CanEnqueueTask() const { return factoryTasks.size() < factories.size() * 2; }
	const std::vector<CRecruitTask*>& GetTasks() const { return factoryTasks; }
	bool IsAssistRequired() const { return isAssistRequired; }
	void ApplySwitchFrame();
	bool IsSwitchTime();
	void RaiseSwitchTime() { isSwitchTime = true; }
	bool IsSwitchAllowed(CCircuitDef* facDef) const;
	CCircuitUnit* NeedUpgrade(unsigned int nanoQueued);
	CCircuitUnit* GetClosestFactory(const springai::AIFloat3& position);
//	CCircuitDef* GetClosestDef(springai::AIFloat3& position, CCircuitDef::RoleT role);

	springai::AIFloat3 GetClosestHaven(CCircuitUnit* unit) const;
	springai::AIFloat3 GetClosestHaven(const springai::AIFloat3& position) const;

	const SSideInfo& GetSideInfo() const;
	const std::vector<SSideInfo>& GetSideInfos() const { return sideInfos; }

	CRecruitTask* UpdateBuildPower(CCircuitUnit* builder, bool isActive);
	CRecruitTask* UpdateFirePower(CCircuitUnit* builder, bool isActive);
	bool IsHighPriority(CAllyUnit* unit) const;

	CCircuitDef* GetFactoryToBuild(springai::AIFloat3 position = -RgtVector,
								   bool isStart = false, bool isReset = false);
	void AddFactory(const CCircuitDef* cdef);
	void DelFactory(const CCircuitDef* cdef);
	CCircuitDef* GetRoleDef(const CCircuitDef* facDef, CCircuitDef::RoleT role) const;
	CCircuitDef* GetLandDef(const CCircuitDef* facDef) const;
	CCircuitDef* GetWaterDef(const CCircuitDef* facDef) const;
	CCircuitDef* GetRepresenter(const CCircuitDef* facDef) const;

	float GetAssistSpeed() const { return GetSideInfo().assistDef->GetBuildSpeed(); }
	float GetAssistRange() const { return GetSideInfo().assistDef->GetBuildDistance(); }

private:
	void EnableFactory(CCircuitUnit* unit);
	void DisableFactory(CCircuitUnit* unit);
	virtual IUnitTask* DefaultMakeTask(CCircuitUnit* unit) override;
	IUnitTask* CreateFactoryTask(CCircuitUnit* unit);
	IUnitTask* CreateAssistTask(CCircuitUnit* unit);

	void Watchdog();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers destroyedHandler;

	std::map<CAllyUnit*, IBuilderTask*> unfinishedUnits;
	std::vector<CRecruitTask*> factoryTasks;  // order matters
	float metalRequire;
	float energyRequire;
	float newFacModM;
	float newFacModE;
	float facModM;
	float facModE;

	std::vector<SSideInfo> sideInfos;

	struct SAssistToFactory {
		std::set<CCircuitUnit*> factories;
		float metalRequire = 0.f;
		float energyRequire = 0.f;
	};
	std::map<CCircuitUnit*, SAssistToFactory> assists;  // nano 1:n factory
	std::vector<springai::AIFloat3> havens;  // position behind factory
	std::map<ICoreUnit::Id, CSRepairTask*> repairUnits;

	CFactoryData* factoryData;
	struct SAssistant {
		std::set<CCircuitUnit*> units;
		float incomeMod;
	};
	struct SFactory {
		SFactory(CCircuitUnit* u, const std::map<CCircuitDef*, SAssistant>& n, unsigned int ns, unsigned int w, CCircuitDef* b,
				 float mi, float ei, float mit, float eit)
			: unit(u), nanos(n), nanoSize(ns), weight(w), builder(b)
			, miRequire(mi), eiRequire(ei), miRequireTotal(mit), eiRequireTotal(eit)
		{}
		CCircuitUnit* unit;
		std::map<CCircuitDef*, SAssistant> nanos;
		unsigned int nanoSize;
		unsigned int weight;
		CCircuitDef* builder;
		float miRequire;
		float eiRequire;
		float miRequireTotal;
		float eiRequireTotal;
	};
	std::vector<SFactory> factories;  // factory 1:n nano
	std::set<CCircuitUnit*> validAir;
	bool isAssistRequired;
	bool isSwitchTime;
	int lastSwitchFrame;
	int noT1FacCount;

	struct SFactoryDef {
		using Tiers = std::map<unsigned, std::vector<float>>;  // tier: probs

		SFactoryDef()
			: landDef(nullptr)
			, waterDef(nullptr)
			, isRequireEnergy(false)
			, nanoCount(0)
		{}

		std::vector<CCircuitDef*> buildDefs;
		Tiers airTiers;
		Tiers landTiers;
		Tiers waterTiers;
		CCircuitDef* landDef;
		CCircuitDef* waterDef;
		std::vector<float> incomes;
		bool isRequireEnergy;
		unsigned int nanoCount;
	};
	std::unordered_map<CCircuitDef::Id, SFactoryDef> factoryDefs;
	float bpRatio;
	float reWeight;

	struct SFireDef {
		CCircuitDef* cdef;
		const std::vector<float>* probs;
		bool isResponse;
		int buildCount;
	};
	int numBatch;
	std::map<CCircuitDef::Id, SFireDef> lastFireDef;  // factory: SFireDef
	void SetLastRequiredDef(CCircuitDef::Id facId, CCircuitDef* cdef,
							const std::vector<float>& probs, bool isResp);
	std::pair<CCircuitDef*, bool> GetLastRequiredDef(CCircuitDef::Id facId,
			const std::vector<float>& probs, const std::function<bool (CCircuitDef*)>& isAvailable);

	struct SRecruitDef {
		CCircuitDef::Id id;
		CRecruitTask::Priority priority;
	};
	SRecruitDef RequiredFireDef(CCircuitUnit* builder, bool isActive);

	const std::vector<float>& GetFacTierProbs(const SFactoryDef& facDef) const;
	CCircuitDef* GetFacRoleDef(CCircuitDef::RoleT role, const SFactoryDef& facDef) const;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
