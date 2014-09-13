/*
 * EconomyManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef ECONOMYMANAGER_H_
#define ECONOMYMANAGER_H_

#include "Module.h"

#include <map>
#include <list>
#include <unordered_set>
#include <functional>

namespace springai {
	class Resource;
}

namespace circuit {

class IConstructTask;

class CEconomyManager: public virtual IModule {
public:
	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);

private:
	void Update();
	CCircuitUnit* FindUnitToAssist(CCircuitUnit* unit);
	void PrepareFactory(CCircuitUnit* unit);
	void ExecuteFactory(CCircuitUnit* unit);
	void PrepareBuilder(CCircuitUnit* unit);
	void ExecuteBuilder(CCircuitUnit* unit);

	using Handlers = std::map<int, std::function<void (CCircuitUnit* unit)>>;
	Handlers finishedHandler;
	Handlers idleHandler;
	Handlers destroyedHandler;
	std::map<CCircuitUnit*, IConstructTask*> unfinishedUnits;
	std::map<IConstructTask*, std::list<CCircuitUnit*>> unfinishedTasks;
	float totalBuildpower;
	springai::Resource* metalRes;
	springai::Resource* energyRes;
	std::list<IConstructTask*> builderTasks;  // owner
	std::list<IConstructTask*> factoryTasks;  // owner

	// TODO: Use or delete
	std::unordered_set<CCircuitUnit*> workers;  // Consider O(n) complexity of insertion in case table rebuilding. reserve/rehash?
};

} // namespace circuit

#endif // ECONOMYMANAGER_H_
