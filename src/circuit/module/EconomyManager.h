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

namespace springai {
	class Resource;
	class Economy;
	class AIFloat3;
	class UnitDef;
}

namespace circuit {

class IBuilderTask;
class CRecruitTask;

class CEconomyManager: public IModule {
public:
	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	IBuilderTask* CreateBuilderTask(CCircuitUnit* unit);
	CRecruitTask* CreateFactoryTask(CCircuitUnit* unit);
	springai::Resource* GetMetalRes() const;
	springai::Resource* GetEnergyRes() const;
	springai::UnitDef* GetMexDef() const;
	springai::AIFloat3 FindBuildPos(CCircuitUnit* unit);


private:
	void Init();
	IBuilderTask* UpdateMetalTasks(const springai::AIFloat3& position);
	IBuilderTask* UpdateEnergyTasks(const springai::AIFloat3& position);
	IBuilderTask* UpdateFactoryTasks(const springai::AIFloat3& position);
	CRecruitTask* UpdateRecruitTasks();
	IBuilderTask* UpdateStorageTasks();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers2 destroyedHandler;

	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* eco;

	struct ClusterInfo {
		CCircuitUnit* factory;
		CCircuitUnit* pylon;
	};
	std::vector<ClusterInfo> clusterInfos;
	float pylonRange;
	int pylonCount, pylonMaxCount;

	springai::UnitDef* mexDef;
	using Energy = struct Energy {
		springai::UnitDef* def;
		float make;
		float cost;
	};
	std::vector<Energy> energyDefs;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_ECONOMYMANAGER_H_
