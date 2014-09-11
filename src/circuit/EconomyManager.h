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

class CEconomyTask;

class CEconomyManager: public virtual IModule {
public:
	CEconomyManager(CCircuitAI* circuit);
	virtual ~CEconomyManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);

private:
	using Handlers = std::map<int, std::function<void (CCircuitUnit* unit)>>;
	Handlers createdHandler;
	Handlers finishedHandler;
	Handlers destroyedHandler;
	std::unordered_set<CCircuitUnit*> workers;  // Consider O(n) complexity of insertion in case table rebuilding. reserve/rehash?
	float totalBuildpower;

	std::list<CEconomyTask*> tasks;  // owner

	void Update();
};

} // namespace circuit

#endif // ECONOMYMANAGER_H_
