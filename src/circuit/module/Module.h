/*
 * Module.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_MODULE_H_
#define SRC_CIRCUIT_MODULE_MODULE_H_

#include "unit/CircuitDef.h"

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

	friend std::ostream& operator<<(std::ostream& os, const IModule& data);
	friend std::istream& operator>>(std::istream& is, IModule& data);
protected:
	virtual void Load(std::istream& is) {}
	virtual void Save(std::ostream& os) const {}

	using Handlers1 = std::unordered_map<CCircuitDef::Id, std::function<void (CCircuitUnit* unit)>>;
	using Handlers2 = std::unordered_map<CCircuitDef::Id, std::function<void (CCircuitUnit* unit, CCircuitUnit* other)>>;
	using EHandlers = std::unordered_map<CCircuitDef::Id, std::function<void (CCircuitUnit* unit, CEnemyUnit* other)>>;

	CCircuitAI* circuit;
};

inline std::ostream& operator<<(std::ostream& os, const IModule& data)
{
	data.Save(os);
	return os;
}

inline std::istream& operator>>(std::istream& is, IModule& data)
{
	data.Load(is);
	return is;
}

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MODULE_H_
