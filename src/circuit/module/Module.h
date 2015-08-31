/*
 * Module.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_MODULE_H_
#define SRC_CIRCUIT_MODULE_MODULE_H_

#include <unordered_map>
#include <functional>

namespace circuit {

class CCircuitAI;
class CCircuitUnit;
class CEnemyUnit;

class IModule {
protected:
	IModule(CCircuitAI* circuit);
public:
	virtual ~IModule();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);
	virtual int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);

protected:
	using Handlers1 = std::unordered_map<int, std::function<void (CCircuitUnit* unit)>>;
	using Handlers2 = std::unordered_map<int, std::function<void (CCircuitUnit* unit, CCircuitUnit* other)>>;
	using EHandlers = std::unordered_map<int, std::function<void (CCircuitUnit* unit, CEnemyUnit* other)>>;

	CCircuitAI* circuit;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MODULE_H_
