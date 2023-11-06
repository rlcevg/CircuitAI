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
class CEnemyInfo;
class IScript;

class IModule {
protected:
	IModule(CCircuitAI* circuit, IScript* script);
public:
	virtual ~IModule();

	CCircuitAI* GetCircuit() const { return circuit; }

	bool InitScript();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker);
	virtual int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);

	friend std::istream& operator>>(std::istream& is, IModule& data);
	friend std::ostream& operator<<(std::ostream& os, const IModule& data);
	void LoadScript(std::istream& is);
	void SaveScript(std::ostream& os) const;
protected:
	virtual void Load(std::istream& is) {}
	virtual void Save(std::ostream& os) const {}

	using Handlers1 = std::unordered_map<CCircuitDef::Id, std::function<void (CCircuitUnit* unit)>>;
	using Handlers2 = std::unordered_map<CCircuitDef::Id, std::function<void (CCircuitUnit* unit, CCircuitUnit* other)>>;
	using EHandlers = std::unordered_map<CCircuitDef::Id, std::function<void (CCircuitUnit* unit, CEnemyInfo* other)>>;

	CCircuitAI* circuit;
	IScript* script;  // owner
};

inline std::istream& operator>>(std::istream& is, IModule& data)
{
	data.Load(is);
	return is;
}

inline std::ostream& operator<<(std::ostream& os, const IModule& data)
{
	data.Save(os);
	return os;
}

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MODULE_H_
