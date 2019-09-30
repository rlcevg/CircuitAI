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
#define HIDDEN_STORAGE	10000.0f

class IBuilderTask;
class CLagrangeInterPol;
class CGameTask;
class CEnergyGrid;

class CEconomyManager: public IModule {
public:
	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

	springai::Resource* GetMetalRes() const { return metalRes; }
	springai::Resource* GetEnergyRes() const { return energyRes; }
	CEnergyGrid* GetEnergyGrid() const { return energyGrid; }
	CCircuitDef* GetMexDef() const { return mexDef; }
	CCircuitDef* GetLowEnergy(const springai::AIFloat3& pos, float& outMake) const;
	CCircuitDef* GetPylonDef() const { return pylonDef; }
	float GetPylonRange() const { return pylonRange; }
	void AddEnergyDefs(const std::set<CCircuitDef*>& buildDefs);  // add available energy defs
	void RemoveEnergyDefs(const std::set<CCircuitDef*>& buildDefs);
	CCircuitDef* GetDefaultDef() const { return defaultDef; }

	void UpdateResourceIncome();
	float GetAvgMetalIncome() const { return metalIncome; }
	float GetAvgEnergyIncome() const { return energyIncome; }
	float GetEcoFactor() const { return ecoFactor; }
	float GetPullMtoS() const { return pullMtoS; }
	float GetMetalPull();
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

	IBuilderTask* MakeEconomyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateReclaimTasks(const springai::AIFloat3& position, CCircuitUnit* unit, bool isNear = true);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks();
	IBuilderTask* UpdateStorageTasks();
	IBuilderTask* UpdatePylonTasks();

	void AddMorphee(CCircuitUnit* unit);
	void RemoveMorphee(CCircuitUnit* unit) { morphees.erase(unit); }
	void UpdateMorph();

private:
	void ReadConfig();
	void Init();
	void OpenStrategy(CCircuitDef* facDef, const springai::AIFloat3& pos);

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

	CCircuitDef* mexDef;
	CCircuitDef* storeDef;
	CCircuitDef* defaultDef;

	// NOTE: MetalManager::SetOpenSpot used by whole allyTeam. Therefore
	//       local spot's state descriptor needed for better expansion
	std::vector<bool> openSpots;  // AI-local metal info
	int mexCount;

	std::set<CCircuitDef*> allEnergyDefs;
	std::set<CCircuitDef*> availEnergyDefs;
	struct SEnergyInfo {
		CCircuitDef* cdef;
		float cost;
		float make;
		float costDivMake;
		int limit;
		bool operator==(const CCircuitDef* d) { return cdef == d; }
	};
	std::vector<SEnergyInfo> energyInfos;
	CLagrangeInterPol* engyPol;

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

	std::vector<float> metalIncomes;
	std::vector<float> energyIncomes;
	int indexRes;
	float metalIncome;
	float energyIncome;
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
	int energyUseFrame;
	float metalPull;
	float energyPull;
	float energyUse;

	std::shared_ptr<CGameTask> morph;
	std::set<CCircuitUnit*> morphees;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
