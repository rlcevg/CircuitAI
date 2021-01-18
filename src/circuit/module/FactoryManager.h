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
	void ResetFactoryPower();
	bool IsResetedFactoryPower() const { return isResetedFactoryPower; }
	float GetFactoryPower() const { return factoryPower; }
	float GetEnergyPower() const { return factoryPower + offsetPower; }
	bool CanEnqueueTask() const { return factoryTasks.size() < factories.size() * 2; }
	const std::vector<CRecruitTask*>& GetTasks() const { return factoryTasks; }
	void ApplySwitchFrame();
	bool IsSwitchTime();
	CCircuitUnit* NeedUpgrade();
	CCircuitUnit* GetClosestFactory(springai::AIFloat3 position);
//	CCircuitDef* GetClosestDef(springai::AIFloat3& position, CCircuitDef::RoleT role);

	springai::AIFloat3 GetClosestHaven(CCircuitUnit* unit) const;
	springai::AIFloat3 GetClosestHaven(const springai::AIFloat3& position) const;

	const SSideInfo& GetSideInfo() const;
	const std::vector<SSideInfo>& GetSideInfos() const { return sideInfos; }

	CCircuitDef* GetAirpadDef(CCircuitDef* builderDef) { return airpadDefs[builderDef->GetId()]; }
	CCircuitDef* GetAssistDef(CCircuitDef* builderDef) { return assistDefs[builderDef->GetId()]; }

	CRecruitTask* UpdateBuildPower(CCircuitUnit* unit, bool isActive);
	CRecruitTask* UpdateFirePower(CCircuitUnit* unit);
	bool IsHighPriority(CAllyUnit* unit) const;

	CCircuitDef* GetFactoryToBuild(springai::AIFloat3 position = -RgtVector,
								   bool isStart = false, bool isReset = false);
	void AddFactory(const CCircuitDef* cdef);
	void DelFactory(const CCircuitDef* cdef);
	CCircuitDef* GetRoleDef(const CCircuitDef* facDef, CCircuitDef::RoleT role) const;
	CCircuitDef* GetLandDef(const CCircuitDef* facDef) const;
	CCircuitDef* GetWaterDef(const CCircuitDef* facDef) const;
	CCircuitDef* GetRepresenter(const CCircuitDef* facDef) const;

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
	float factoryPower;  // related to metal
	float offsetPower;
	bool isResetedFactoryPower;

	std::vector<SSideInfo> sideInfos;

	std::unordered_map<CCircuitDef::Id, CCircuitDef*> airpadDefs;  // builder: pad
	std::unordered_map<CCircuitDef::Id, CCircuitDef*> assistDefs;  // builder: assist

	std::map<CCircuitUnit*, std::set<CCircuitUnit*>> assists;  // nano 1:n factory
	std::vector<springai::AIFloat3> havens;  // position behind factory
	std::map<ICoreUnit::Id, IBuilderTask*> repairedUnits;

	CFactoryData* factoryData;
	struct SFactory {
		SFactory(CCircuitUnit* u, const std::set<CCircuitUnit*>& n, unsigned int w, CCircuitDef* b)
			: unit(u)
			, nanos(n)
			, weight(w)
			, builder(b)
		{}
		CCircuitUnit* unit;
		std::set<CCircuitUnit*> nanos;
		unsigned int weight;
		CCircuitDef* builder;
	};
	std::vector<SFactory> factories;  // facory 1:n nano
	std::set<CCircuitUnit*> validAir;
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

	const std::vector<float>& GetFacTierProbs(const SFactoryDef& facDef) const;
	CCircuitDef* GetFacRoleDef(CCircuitDef::RoleT role, const SFactoryDef& facDef) const;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
