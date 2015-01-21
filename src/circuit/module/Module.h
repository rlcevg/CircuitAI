/*
 * Module.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef MODULE_H_
#define MODULE_H_

#include <unordered_map>
#include <functional>

namespace circuit {

class CCircuitAI;
class CCircuitUnit;

class IModule {
protected:
	IModule(CCircuitAI* circuit);
public:
	virtual ~IModule();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int EnemyEnterLOS(CCircuitUnit* unit);

protected:
	using Handlers1 = std::unordered_map<int, std::function<void (CCircuitUnit* unit)>>;
	using Handlers2 = std::unordered_map<int, std::function<void (CCircuitUnit* unit, CCircuitUnit* other)>>;

	CCircuitAI* circuit;
};

} // namespace circuit

#endif // MODULE_H_
