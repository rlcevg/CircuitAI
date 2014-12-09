/*
 * EconomyManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef ECONOMYMANAGER_H_
#define ECONOMYMANAGER_H_

#include "Module.h"
#include "BuilderManager.h"
#include "FactoryManager.h"
#include "TerrainAnalyzer.h"

#include "AIFloat3.h"

#include <vector>
#include <unordered_map>
#include <functional>

namespace springai {
	class Resource;
	class Economy;
}

namespace circuit {

class CBuilderTask;
class CFactoryTask;

class CEconomyManager: public virtual IModule {
public:
	CEconomyManager(CCircuitAI* circuit, CBuilderManager* builderManager, CFactoryManager* factoryManager);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	CBuilderTask* CreateBuilderTask(CCircuitUnit* unit);
	CFactoryTask* CreateFactoryTask(CCircuitUnit* unit);
	springai::Resource* GetMetalRes();
	springai::Resource* GetEnergyRes();
	springai::AIFloat3 FindBuildPos(CCircuitUnit* unit);

private:
	void Init();
	CBuilderTask* UpdateMetalTasks();
	CBuilderTask* UpdateEnergyTasks();
	CBuilderTask* UpdateBuilderTasks();
	CFactoryTask* UpdateFactoryTasks();

	using Handlers1 = std::unordered_map<int, std::function<void (CCircuitUnit* unit)>>;
	using Handlers2 = std::unordered_map<int, std::function<void (CCircuitUnit* unit, CCircuitUnit* other)>>;
	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 destroyedHandler;

	springai::Resource* metalRes;
	springai::Resource* energyRes;
	springai::Economy* eco;
	CBuilderManager* builderManager;
	CFactoryManager* factoryManager;

	struct ClusterInfo {
		CCircuitUnit* factory;
		CCircuitUnit* pylon;
	};
	std::vector<ClusterInfo> clusterInfo;
	int solarCount;
	int fusionCount;
	float pylonRange;
//	float singuRange;
};

} // namespace circuit

#endif // ECONOMYMANAGER_H_
