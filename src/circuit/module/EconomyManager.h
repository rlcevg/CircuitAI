/*
 * EconomyManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
#define SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_

#include "module/Module.h"
#include "util/AvailList.h"

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
class IMainJob;
class CBFactoryTask;
class CEnergyGrid;

class CEconomyManager: public IModule {
public:
	friend class CEconomyScript;

	struct SEnergyCond {
		float score = -1.f;
		float metalIncome = -1.f;  // condition
		float energyIncome = -1.f;  // condition
		int limit = 0;
	};
	struct SSideInfo {
		CCircuitDef* mexDef;
		CCircuitDef* geoDef;
		CCircuitDef* mohoMexDef;
		CCircuitDef* defaultDef;

		std::unordered_map<CCircuitDef*, SEnergyCond> engyLimits;
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
	float GetClusterRange() const { return clusterRange; }
	float GetPylonRange() const { return pylonRange; }
	CCircuitDef* GetLowEnergy(const springai::AIFloat3& pos, float& outMake, const CCircuitUnit* builder = nullptr) const;
	void AddEconomyDefs(const std::set<CCircuitDef*>& buildDefs);  // add available economy defs
	void RemoveEconomyDefs(const std::set<CCircuitDef*>& buildDefs);

	const SSideInfo& GetSideInfo() const;
	const std::vector<SSideInfo>& GetSideInfos() const { return sideInfos; }

	const std::vector<CCircuitDef*> GetMexDefs(CCircuitDef* builderDef) { return mexDefs[builderDef->GetId()]; }
	CCircuitDef* GetDefaultDef(CCircuitDef* builderDef) { return defaultDefs[builderDef->GetId()]; }
	CCircuitDef* GetPylonDef() const { return pylonDef; }

	bool IsFactoryDefAvail(CCircuitDef* buildDef) const { return factoryDefs.IsAvail(buildDef); }

	void UpdateResourceIncome();
	float GetAvgMetalIncome() const { return metal.income; }
	float GetAvgEnergyIncome() const { return energy.income; }
	float GetEcoFactor() const { return ecoFactor; }
	float GetEcoEM() const { return ecoEMRatio; }
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
	bool IsEnergyRequired() const { return isEnergyRequired; }
	void ClearEnergyRequired() { isEnergyRequired = false; }
	bool IsExcessed() const { return metalProduced > metalUsed; }
	int GetBuildDelay() const { return buildDelay; }

	bool IsAllyOpenSpot(int spotId) const;
	bool IsOpenSpot(int spotId) const { return mexSpots[spotId].isOpen && (mexCount < mexMax); }
	void SetOpenSpot(int spotId, bool value);
	bool IsUpgradingSpot(int spotId) const { return mexSpots[spotId].isUp; }
	void SetUpgradingSpot(int spotId, bool value) { mexSpots[spotId].isUp = value; }
	bool IsOpenGeoSpot(int spotId) const { return geoSpots[spotId]; }
	void SetOpenGeoSpot(int spotId, bool value) { geoSpots[spotId] = value; }
	bool IsIgnorePull(const IBuilderTask* task) const;
	bool IsIgnoreStallingPull(const IBuilderTask* task) const;

	IBuilderTask* MakeEconomyTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
	IBuilderTask* UpdateReclaimTasks(const springai::AIFloat3& position, CCircuitUnit* unit, bool isNear = true);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateGeoTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks();
	IBuilderTask* UpdateStorageTasks();
	IBuilderTask* UpdatePylonTasks();
	void StartFactoryTask(const float seconds);

	void AddMorphee(CCircuitUnit* unit);
	void RemoveMorphee(CCircuitUnit* unit) { morphees.erase(unit); }
	void UpdateMorph();

	void OpenStrategy(const CCircuitDef* facDef, const springai::AIFloat3& pos);

private:
	bool CheckAssistRequired(const springai::AIFloat3& position, CCircuitUnit* unit, IBuilderTask*& outTask);
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
	float clusterRange;
	float pylonRange;
	CCircuitDef* pylonDef;  // TODO: Move into CEnergyGrid?

	std::vector<SSideInfo> sideInfos;

	struct SStoreExt {
		float storage;
	};
	CAvailList<SStoreExt> storeMDefs, storeEDefs;

	std::unordered_map<CCircuitDef::Id, std::vector<CCircuitDef*>> mexDefs;  // builder: mex
	std::unordered_map<CCircuitDef::Id, CCircuitDef*> defaultDefs;  // builder: default

	// NOTE: MetalManager::SetOpenSpot used by whole allyTeam. Therefore
	//       local spot's state descriptor needed for better expansion
	struct SMSpot {
		bool isOpen : 1;
		bool isUp : 1;  // spot being upgraded
	};
	std::vector<SMSpot> mexSpots;  // AI-local metal info
	int mexCount;
	std::vector<bool> geoSpots;

	struct SMetalExt {
		float speed;
	};
	CAvailList<SMetalExt> metalDefs;

	struct SConvertExt {
		float make;
	};
	CAvailList<SConvertExt> convertDefs;

	struct SGeoExt {
		float make;
	};
	CAvailList<SGeoExt> geoDefs;

	float costRatio;
	struct SEnergyExt {
		float make;
		SEnergyCond cond;  // condition
	};
	CAvailList<SEnergyExt> energyDefs;

	float ecoStep;
	float ecoFactor;
	float ecoEMRatio;  // energy to metal ratio

	struct SEnergyFactorInfo {
		float startFactor;
		float endFactor;
		int startFrame;
		int endFrame;
		float fraction;
	} efInfo;
	float energyFactor;

	std::vector<float> metalIncomes;
	std::vector<float> energyIncomes;
	int indexRes;
	float metalProduced;
	float metalUsed;
	float metalMod;
	int mexMax;
	int buildDelay;
	unsigned numMexUp;

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
	bool isEnergyRequired;

	int metalPullFrame;
	int energyPullFrame;
	struct SResourceInfo {
		int pullFrame;
		float current;
		float storage;
		float pull;
		float income;
	} metal, energy;
	float energyUse;

	struct SAssistExt {
	};
	CAvailList<SAssistExt> assistDefs;

	struct SFactoryExt {
	};
	CAvailList<SFactoryExt> factoryDefs;

	std::shared_ptr<IMainJob> startFactory;
	CBFactoryTask* factoryTask;

	std::shared_ptr<IMainJob> morph;
	std::set<CCircuitUnit*> morphees;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
