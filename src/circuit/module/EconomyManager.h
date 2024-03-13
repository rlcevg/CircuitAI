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
		CCircuitDef* defaultDef;

		std::unordered_map<CCircuitDef*, SEnergyCond> engyLimits;
	};

	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

private:
	void ReadConfig(float& outMinEInc);
	void InitEconomyScores(const std::vector<CCircuitDef*>&& builders);
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

	CCircuitDef* GetDefaultDef(CCircuitDef* conDef) { return defaultDefs[conDef->GetId()]; }
	CCircuitDef* GetPylonDef() const { return pylonDef; }

	void AddAssistDef(CCircuitDef* cdef) { assistDefs.AddDef(cdef); cdef->SetIsAssist(true); }
	void AddFactoryDef(CCircuitDef* cdef) { factoryDefs.AddDef(cdef); }
	bool IsFactoryDefAvail(CCircuitDef* buildDef) const { return factoryDefs.IsAvail(buildDef); }

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
	bool IsMetalEmpty() const { return isMetalEmpty; }
	bool IsMetalFull() const { return isMetalFull; }
	bool IsEnergyStalling() const { return isEnergyStalling; }
	bool IsEnergyEmpty() const { return isEnergyEmpty; }
	bool IsEnergyFull() const { return isEnergyFull; }
//	bool IsEnergyRequired() const { return isEnergyRequired; }
	void ClearEnergyRequired() { isEnergyRequired = false; }
	bool IsExcessed() const { return metalProduced > metalUsed; }
	int GetBuildDelay() const { return buildDelay; }

	bool IsAllyOpenMexSpot(int spotId) const;
	bool IsOpenMexSpot(int spotId) const;
	void SetOpenMexSpot(int spotId, bool value);
	bool IsUpgradingMexSpot(int spotId) const { return mexSpots[spotId].isUp; }
	void SetUpgradingMexSpot(int spotId, bool value) { mexSpots[spotId].isUp = value; }
	bool IsOpenGeoSpot(int spotId) const { return geoSpots[spotId].isOpen; }
	void SetOpenGeoSpot(int spotId, bool value) { geoSpots[spotId].isOpen = value; }
	bool IsUpgradingGeoSpot(int spotId) const { return geoSpots[spotId].isUp; }
	void SetUpgradingGeoSpot(int spotId, bool value) { geoSpots[spotId].isUp = value; }
	bool IsIgnorePull(const IBuilderTask* task) const;
	bool IsIgnoreStallingPull(const IBuilderTask* task) const;
	void CorrectResourcePull(float metal, float energy);
	float GetMetalPullCor() const { return metal.pull - metalPullCor; }
	float GetEnergyPullCor() const { return energy.pull - energyPullCor; }
	bool IsEnoughEnergy(IBuilderTask const* task, CCircuitDef const* conDef, float mod = 1.f) const;

	IBuilderTask* MakeEconomyTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
	IBuilderTask* UpdateReclaimTasks(const springai::AIFloat3& position, CCircuitUnit* unit, bool isNear = true);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateGeoTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks();
	IBuilderTask* UpdateStorageTasks();
	IBuilderTask* UpdatePylonTasks();
	IBuilderTask* CheckMobileAssistRequired(const springai::AIFloat3& position, CCircuitUnit* unit);
	void StartFactoryJob(const float seconds);
	CBFactoryTask* PickNextFactory(const springai::AIFloat3& position, bool isStart);

	void AddMorphee(CCircuitUnit* unit);
	void RemoveMorphee(CCircuitUnit* unit) { morphees.erase(unit); }
	void UpdateMorph();

	void AddFactoryInfo(CCircuitUnit* unit);
	void DelFactoryInfo(CCircuitUnit* unit);

private:
	bool CheckAirpadRequired(const springai::AIFloat3& position, CCircuitUnit* unit, IBuilderTask*& outTask);
	bool CheckAssistRequired(const springai::AIFloat3& position, CCircuitUnit* unit, IBuilderTask*& outTask);
	float GetStorage(springai::Resource* res);
	void UpdateResourceIncome();
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
	float pylonLinkInc;
	CCircuitDef* pylonDef;  // TODO: Move into CEnergyGrid?

	std::vector<SSideInfo> sideInfos;

	struct SStoreExt {
		float storage;
	};
	CAvailList<SStoreExt> storeMDefs, storeEDefs;

	std::unordered_map<CCircuitDef::Id, CCircuitDef*> defaultDefs;  // builder: default

	// NOTE: MetalManager::SetOpenSpot used by whole allyTeam. Therefore
	//       local spot's state descriptor needed for better expansion
	struct SResSpot {
		bool isOpen : 1;
		bool isUp : 1;  // spot being upgraded
	};
	std::vector<SResSpot> mexSpots;  // AI-local metal info
	int mexCount;
	std::vector<SResSpot> geoSpots;

	struct SMetalExt {
		float speed;
	};
	CAvailList<SMetalExt> metalDefs;

	struct SConvertExt {
		float energyUse;
		float make;
	};
	CAvailList<SConvertExt> convertDefs;
	void ReclaimOldConvert(const SConvertExt* convertExt);

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
	void ReclaimOldEnergy(const SEnergyExt* energyExt);

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

	struct SBuildDelayInfo {
		int startDelay;
		int endDelay;
		int startFrame;
		int endFrame;
		float fraction;
	} bdInfo;
	int buildDelay;  // frames

	std::vector<float> metalIncomes;
	std::vector<float> energyIncomes;
	int indexRes;
	float metalProduced;
	float metalUsed;
	float metalMod;
	int mexMax;
	bool isAllyMexMax;
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

	bool isMetalEmpty;
	bool isMetalFull;
	bool isEnergyStalling;
	bool isEnergyEmpty;
	bool isEnergyFull;
	bool isEnergyRequired;
	float reclConvertEff;
	float reclEnergyEff;

	int metalPullFrame;
	int energyPullFrame;
	struct SResourceInfo {
		int pullFrame;
		float current;
		float storage;
		float pull;
		float income;
	} metal, energy;
	int metalPullCorFrame;
	float metalPullCor;
	int energyPullCorFrame;
	float energyPullCor;

	struct SAirpadExt {
	};
	CAvailList<SAirpadExt> airpadDefs;
	int airpadCount;

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
