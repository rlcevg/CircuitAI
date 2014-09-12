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
	virtual int CommandFinished(CCircuitUnit* unit, int commandTopicId);

	void InformUnfinished(CCircuitUnit* unit, CCircuitUnit* builder);  // Candidate for NotificationCenter message
	void InformFinished(CCircuitUnit* unit);  // Candidate for NotificationCenter message
	void InformDestroyed(CCircuitUnit* unit);  // Candidate for NotificationCenter message

private:
	void Update();
	void PrepareFactory(CCircuitUnit* unit);
	void ExecuteFactory(CCircuitUnit* unit);

	using Handlers2 = std::map<int, std::function<void (CCircuitUnit* unit, CCircuitUnit* builder)>>;
	using Handlers1 = std::map<int, std::function<void (CCircuitUnit* unit)>>;
	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers1 destroyedHandler;
	Handlers1 commandHandler;
	std::unordered_set<CCircuitUnit*> workers;  // Consider O(n) complexity of insertion in case table rebuilding. reserve/rehash?
	std::map<CCircuitUnit*, IConstructTask*> unfinished;
	float totalBuildpower;

	std::list<IConstructTask*> builderTasks;  // owner
	std::list<IConstructTask*> factoryTasks;  // owner
};

} // namespace circuit

#endif // ECONOMYMANAGER_H_
