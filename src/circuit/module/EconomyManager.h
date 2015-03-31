/*
 * EconomyManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
#define SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_

#include "module/Module.h"
#include "static/MetalData.h"

#include "AIFloat3.h"

#include <vector>
#include <list>
#include <set>

namespace springai {
	class Resource;
	class Economy;
	class AIFloat3;
	class UnitDef;
}

namespace circuit {

class IBuilderTask;
class CRecruitTask;
class CLagrangeInterPol;

class CEconomyManager: public IModule {
public:
	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	IBuilderTask* CreateBuilderTask(CCircuitUnit* unit);
	CRecruitTask* CreateFactoryTask(CCircuitUnit* unit);
	IBuilderTask* CreateAssistTask(CCircuitUnit* unit);
	springai::Resource* GetMetalRes() const;
	springai::Resource* GetEnergyRes() const;
	springai::UnitDef* GetMexDef() const;
	springai::AIFloat3 FindBuildPos(CCircuitUnit* unit);
	void AddAvailEnergy(const std::set<springai::UnitDef*>& buildDefs);  // add available energy defs
	void RemoveAvailEnergy(const std::set<springai::UnitDef*>& buildDefs);

	void UpdateResourceIncome();
	float GetAvgMetalIncome() const;
	float GetAvgEnergyIncome() const;
	float GetEcoFactor() const;

	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	CRecruitTask* UpdateRecruitTasks();
	IBuilderTask* UpdateStorageTasks();

private:
	void Init();
	void LinkCluster(int index);

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers2 destroyedHandler;

	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* eco;

	struct SClusterInfo {
		CCircuitUnit* factory;
		CCircuitUnit* pylon;
	};
	std::vector<SClusterInfo> clusterInfos;
	float pylonRange;
	int pylonCount, pylonMaxCount;

	springai::UnitDef* mexDef;
	std::set<springai::UnitDef*> allEnergyDefs;
	std::set<springai::UnitDef*> availEnergyDefs;
	using SEnergyInfo = struct {
		springai::UnitDef* def;
		float cost;
		float costDivMake;
		int limit;
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

	std::vector<CMetalData::Edge> spanningTree;
	CMetalData::Graph spanningGraph;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
