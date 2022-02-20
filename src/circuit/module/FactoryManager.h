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

class CFactoryManager: public IUnitModule {
public:
	friend class CFactoryScript;

	struct SSideInfo {
		CCircuitDef* airpadDef;
		CCircuitDef* assistDef;
	};

	CFactoryManager(CCircuitAI* circuit);
	virtual ~CFactoryManager();

private:
	void ReadConfig();
	void Init();
public:
	void Release();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	CRecruitTask* EnqueueTask(CRecruitTask::Priority priority,
							  CCircuitDef* buildDef,
							  const springai::AIFloat3& position,
							  CRecruitTask::RecruitType type,
							  float radius);
	IUnitTask* EnqueueWait(bool stop, int timeout);
	IBuilderTask* EnqueueReclaim(IBuilderTask::Priority priority,
								 const springai::AIFloat3& position,
								 float radius,
								 int timeout = 0);
	IBuilderTask* EnqueueRepair(IBuilderTask::Priority priority,
								CAllyUnit* target);
private:
	virtual void DequeueTask(IUnitTask* task, bool done = false) override;

public:
	virtual void FallbackTask(CCircuitUnit* unit) override;

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
	void ClearAssistRequired() { isAssistRequired = false; }
	void ApplySwitchFrame();
	bool IsSwitchTime();
	void RaiseSwitchTime() { isSwitchTime = true; }
	bool IsSwitchAllowed(CCircuitDef* facDef) const;
	CCircuitUnit* NeedUpgrade(unsigned int nanoQueued);
	CCircuitUnit* GetClosestFactory(springai::AIFloat3 position);
//	CCircuitDef* GetClosestDef(springai::AIFloat3& position, CCircuitDef::RoleT role);

	springai::AIFloat3 GetClosestHaven(CCircuitUnit* unit) const;
	springai::AIFloat3 GetClosestHaven(const springai::AIFloat3& position) const;

	const SSideInfo& GetSideInfo() const;
	const std::vector<SSideInfo>& GetSideInfos() const { return sideInfos; }

	CCircuitDef* GetAirpadDef(CCircuitDef* builderDef) { return airpadDefs[builderDef->GetId()]; }

	CRecruitTask* UpdateBuildPower(CCircuitUnit* builder, bool isActive);
	CRecruitTask* UpdateFirePower(CCircuitUnit* builder);
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
	void UpdateIdle();
	void UpdateFactory();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers destroyedHandler;

	std::map<CAllyUnit*, IBuilderTask*> unfinishedUnits;
	std::vector<CRecruitTask*> factoryTasks;  // order matters
	std::vector<IUnitTask*> updateTasks;  // owner
	unsigned int updateIterator;
	float metalRequire;
	float energyRequire;
	float newFacModM;
	float newFacModE;
	float facModM;
	float facModE;

	std::vector<SSideInfo> sideInfos;

	std::unordered_map<CCircuitDef::Id, CCircuitDef*> airpadDefs;  // builder: pad

	struct SAssistToFactory {
		std::set<CCircuitUnit*> factories;
		float metalRequire = 0.f;
		float energyRequire = 0.f;
	};
	std::map<CCircuitUnit*, SAssistToFactory> assists;  // nano 1:n factory
	std::vector<springai::AIFloat3> havens;  // position behind factory
	std::map<ICoreUnit::Id, IBuilderTask*> repairedUnits;

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
	SRecruitDef RequiredFireDef(CCircuitUnit* builder);

	const std::vector<float>& GetFacTierProbs(const SFactoryDef& facDef) const;
	CCircuitDef* GetFacRoleDef(CCircuitDef::RoleT role, const SFactoryDef& facDef) const;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
