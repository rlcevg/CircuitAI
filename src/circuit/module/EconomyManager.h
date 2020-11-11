/*
 * EconomyManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
#define SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_

#include "module/Module.h"

#include "AIFloat3.h"

#include <set>
#include <memory>

namespace springai {
	class Resource;
	class Economy;
}

namespace circuit {

#define INCOME_SAMPLES	5
#define HIDDEN_STORAGE	.0f

extern const char* RES_NAME_METAL;
extern const char* RES_NAME_ENERGY;

class IBuilderTask;
class CGameTask;
class CEnergyGrid;

class CEconomyManager: public IModule {
public:
	friend class CEconomyScript;
	struct SSideInfo {
		CCircuitDef* mexDef;
		CCircuitDef* mohoMexDef;
		CCircuitDef* defaultDef;

		std::unordered_map<CCircuitDef*, int> engyLimits;
	};

	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

private:
	void ReadConfig();
	void Init();

public:
	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) override;

	springai::Resource* GetMetalRes() const { return metalRes; }
	springai::Resource* GetEnergyRes() const { return energyRes; }
	CEnergyGrid* GetEnergyGrid() const { return energyGrid; }
	float GetPylonRange() const { return pylonRange; }
	CCircuitDef* GetLowEnergy(const springai::AIFloat3& pos, float& outMake, CCircuitUnit* builder = nullptr) const;
	void AddEconomyDefs(const std::set<CCircuitDef*>& buildDefs);  // add available economy defs
	void RemoveEconomyDefs(const std::set<CCircuitDef*>& buildDefs);

	const SSideInfo& GetSideInfo() const;
	const std::vector<SSideInfo>& GetSideInfos() const { return sideInfos; }

	CCircuitDef* GetMexDef(CCircuitDef* builderDef) { return mexDefs[builderDef->GetId()]; }
	CCircuitDef* GetDefaultDef(CCircuitDef* builderDef) { return defaultDefs[builderDef->GetId()]; }
	CCircuitDef* GetPylonDef() const { return pylonDef; }

	void UpdateResourceIncome();
	float GetAvgMetalIncome() const { return metal.income; }
	float GetAvgEnergyIncome() const { return energy.income; }
	float GetEcoFactor() const { return ecoFactor; }
	float GetPullMtoS() const { return pullMtoS; }
	float GetMetalCur();
	float GetMetalStore();
	float GetMetalPull();
	float GetEnergyCur();
	float GetEnergyStore();
	float GetEnergyPull();
	float GetEnergyUse();
	bool IsMetalEmpty();
	bool IsMetalFull();
	bool IsEnergyStalling();
	bool IsEnergyEmpty();
	bool IsExcessed() const { return metalProduced > metalUsed; }
	int GetBuildDelay() const { return buildDelay; }

	bool IsAllyOpenSpot(int spotId) const;
	bool IsOpenSpot(int spotId) const { return openSpots[spotId] && (mexCount < mexMax); }
	void SetOpenSpot(int spotId, bool value);
	bool IsIgnorePull(const IBuilderTask* task) const;
	bool IsIgnoreStallingPull(const IBuilderTask* task) const;

	IBuilderTask* MakeEconomyTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
	IBuilderTask* UpdateReclaimTasks(const springai::AIFloat3& position, CCircuitUnit* unit, bool isNear = true);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks();
	IBuilderTask* UpdateStorageTasks();
	IBuilderTask* UpdatePylonTasks();

	void AddMorphee(CCircuitUnit* unit);
	void RemoveMorphee(CCircuitUnit* unit) { morphees.erase(unit); }
	void UpdateMorph();

	void OpenStrategy(const CCircuitDef* facDef, const springai::AIFloat3& pos);

private:
	float GetStorage(springai::Resource* res);
	void UpdateEconomy();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	EHandlers destroyedHandler;

	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* economy;
	CEnergyGrid* energyGrid;

	struct SClusterInfo {
		CCircuitUnit* factory;
		int metalFrame;
	};
	std::vector<SClusterInfo> clusterInfos;
	float pylonRange;
	CCircuitDef* pylonDef;  // TODO: Move into CEnergyGrid?

	std::vector<SSideInfo> sideInfos;

	struct SStoreInfo {
		CCircuitDef* cdef;
		float storage;
		float score;
		bool operator==(const CCircuitDef* d) { return cdef == d; }
	};
	struct SStoreDefs {
		std::set<CCircuitDef*> all;
		std::set<CCircuitDef*> avail;
		std::vector<SStoreInfo> infos;  // sorted high-score first
	} storeMDefs, storeEDefs;
	void AddStoreDefs(const std::set<CCircuitDef*>& buildDefs, SStoreDefs& defsInfo, std::function<float (CCircuitDef*)> storeFunc);  // add available store defs
	void RemoveStoreDefs(const std::set<CCircuitDef*>& buildDefs, SStoreDefs& defsInfo);

	std::unordered_map<CCircuitDef::Id, CCircuitDef*> mexDefs;  // builder: mex
	std::unordered_map<CCircuitDef::Id, CCircuitDef*> defaultDefs;  // builder: default

	// NOTE: MetalManager::SetOpenSpot used by whole allyTeam. Therefore
	//       local spot's state descriptor needed for better expansion
	std::vector<bool> openSpots;  // AI-local metal info
	int mexCount;

	struct SEnergyInfo {
		CCircuitDef* cdef;
		float make;
		float score;
		int limit;
		bool operator==(const CCircuitDef* d) { return cdef == d; }
	};
	struct SEnergyDefs {
		std::set<CCircuitDef*> all;
		std::set<CCircuitDef*> avail;
		std::vector<SEnergyInfo> infos;  // sorted high-score first
	} energyDefs;
	void AddEnergyDefs(const std::set<CCircuitDef*>& buildDefs);  // add available energy defs
	void RemoveEnergyDefs(const std::set<CCircuitDef*>& buildDefs);

	float ecoStep;
	float ecoFactor;
	int switchTime;
	int lastFacFrame;

	struct SEnergyFactorInfo {
		float startFactor;
		float endFactor;
		int startFrame;
		int endFrame;
		float fraction;
	} efInfo;
	float energyFactor;
	float costRatio;

	std::vector<float> metalIncomes;
	std::vector<float> energyIncomes;
	int indexRes;
	float metalProduced;
	float metalUsed;
	float metalMod;
	int mexMax;
	int buildDelay;

	struct SPullMtoS {
		float pull;
		int mex;
		float fraction;
		inline bool operator< (const SPullMtoS& rhs) { return mex < rhs.mex; }
		inline bool operator() (const SPullMtoS& lhs, const int rhs) { return lhs.mex < rhs; }
	};
	std::vector<SPullMtoS> mspInfos;
	float pullMtoS;  // mobile to static metal pull ratio

	int ecoFrame;
	bool isMetalEmpty;
	bool isMetalFull;
	bool isEnergyStalling;
	bool isEnergyEmpty;

	int metalPullFrame;
	int energyPullFrame;
	struct ResourceInfo {
		int pullFrame;
		float current;
		float storage;
		float pull;
		float income;
	} metal, energy;
	float energyUse;

	std::shared_ptr<CGameTask> morph;
	std::set<CCircuitUnit*> morphees;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
