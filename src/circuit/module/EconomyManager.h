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
	class TeamRulesParam;
}

namespace circuit {

class IBuilderTask;
class CRecruitTask;
class CLagrangeInterPol;
class CCircuitDef;
class CGameTask;

class CEconomyManager: public IModule {
public:
	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

	IBuilderTask* CreateBuilderTask(const springai::AIFloat3& position, CCircuitUnit* unit);
	CRecruitTask* CreateFactoryTask(CCircuitUnit* unit);
	IBuilderTask* CreateAssistTask(CCircuitUnit* unit);
	springai::Resource* GetMetalRes() const { return metalRes; }
	springai::Resource* GetEnergyRes() const { return energyRes; }
	CCircuitDef* GetMexDef() const { return mexDef; }
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

	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateReclaimTasks(const springai::AIFloat3& position, CCircuitUnit* unit);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks();
	CRecruitTask* UpdateRecruitTasks();
	IBuilderTask* UpdateStorageTasks();
	IBuilderTask* UpdatePylonTasks();
	void UpdateMorph();

private:
	void Init();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	EHandlers destroyedHandler;

	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* economy;

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

	// TODO: Didn't see any improvements. Remove avg?
	std::vector<float> metalIncomes;
	std::vector<float> energyIncomes;
	int indexRes;
	float metalIncome;
	float energyIncome;

	// Economy metal+energy params
	springai::TeamRulesParam* empParam, *eepParam;
	springai::TeamRulesParam* odeiParam, *odecParam/*, *odeoParam, *odteParam, *odaParam*/;

	int emptyFrame;
	int fullFrame;
	int stallingFrame;
	bool isMetalEmpty;
	bool isMetalFull;
	bool isEnergyStalling;

	int metalPullFrame;
	int energyPullFrame;
	float metalPull;
	float energyPull;

	std::shared_ptr<CGameTask> morph;
	std::set<CCircuitUnit*> morphees;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
