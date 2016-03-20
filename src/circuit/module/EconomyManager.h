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

#include <vector>
#include <list>
#include <set>
#include <memory>

namespace springai {
	class Resource;
	class Economy;
}

namespace circuit {

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
	CCircuitDef* GetLowEnergy(const springai::AIFloat3& pos) const;
	CCircuitDef* GetPylonDef() const { return pylonDef; }
	float GetPylonRange() const { return pylonRange; }
	void AddEnergyDefs(const std::set<CCircuitDef*>& buildDefs);  // add available energy defs
	void RemoveEnergyDefs(const std::set<CCircuitDef*>& buildDefs);

	void UpdateResourceIncome();
	float GetAvgMetalIncome() const { return metalIncome; }
	float GetAvgEnergyIncome() const { return energyIncome; }
	float GetEcoFactor() const { return ecoFactor; }
	float GetMetalPull();
	float GetEnergyPull();
	bool IsMetalEmpty();
	bool IsMetalFull();
	bool IsEnergyStalling();
	bool IsEnergyEmpty();

	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateReclaimTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
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

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	EHandlers destroyedHandler;

	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* economy;
	CEnergyGrid* energyGrid;

	struct SClusterInfo {
		CCircuitUnit* factory;
	};
	std::vector<SClusterInfo> clusterInfos;
	float pylonRange;
	CCircuitDef* pylonDef;  // TODO: Move into CEnergyGrid?

	CCircuitDef* mexDef;
	std::set<CCircuitDef*> allEnergyDefs;
	std::set<CCircuitDef*> availEnergyDefs;
	struct SEnergyInfo {
		CCircuitDef* cdef;
		float cost;
		float costDivMake;
		int limit;
		bool operator==(const CCircuitDef* d) { return cdef == d; }
	};
	std::list<SEnergyInfo> energyInfos;
	CLagrangeInterPol* engyPol;

	float ecoFactor;
	float incomeFactor;

	// TODO: Didn't see any improvements. Remove avg?
	std::vector<float> metalIncomes;
	std::vector<float> energyIncomes;
	int indexRes;
	float metalIncome;
	float energyIncome;

	void UpdateEconomy();
	int ecoFrame;
	bool isMetalEmpty;
	bool isMetalFull;
	bool isEnergyStalling;
	bool isEnergyEmpty;

	int metalPullFrame;
	int energyPullFrame;
	float metalPull;
	float energyPull;

	std::shared_ptr<CGameTask> morph;
	std::set<CCircuitUnit*> morphees;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
