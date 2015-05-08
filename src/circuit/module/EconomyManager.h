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

namespace springai {
	class Resource;
	class Economy;
}

namespace circuit {

class IBuilderTask;
class CRecruitTask;
class CLagrangeInterPol;
class CCircuitDef;

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
	CCircuitDef* GetMexDef() const;
	CCircuitDef* GetPylonDef() const;
	float GetPylonRange() const;
	springai::AIFloat3 FindBuildPos(CCircuitUnit* unit);
	void AddEnergyDefs(const std::set<CCircuitDef*>& buildDefs);  // add available energy defs
	void RemoveEnergyDefs(const std::set<CCircuitDef*>& buildDefs);

	void UpdateResourceIncome();
	float GetAvgMetalIncome() const;
	float GetAvgEnergyIncome() const;
	float GetEcoFactor() const;
	bool IsMetalFull() const;
	bool IsMetalEmpty() const;
	bool IsEnergyStalling() const;

	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position, CCircuitUnit* unit = nullptr);
	CRecruitTask* UpdateRecruitTasks();
	IBuilderTask* UpdateStorageTasks();

private:
	void Init();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers2 destroyedHandler;

	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* eco;

	struct SClusterInfo {
		CCircuitUnit* factory;
		CCircuitUnit* pylon;  // TODO: Remove?
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
	};
	std::list<SEnergyInfo> energyInfos;
	CLagrangeInterPol* engyPol;

	float ecoFactor;
	int maxReclaimers;

	// TODO: Didn't see any improvements. Remove avg?
	std::vector<float> metalIncomes;
	std::vector<float> energyIncomes;
	int indexRes;
	float metalIncome;
	float energyIncome;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
