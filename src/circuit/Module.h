/*
 * Module.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef MODULE_H_
#define MODULE_H_

namespace circuit {

class CCircuitAI;
class CCircuitUnit;

class IModule {
public:
	virtual ~IModule();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);
	virtual int CommandFinished(CCircuitUnit* unit, int commandTopicId);

protected:
	IModule(CCircuitAI* circuit);

	CCircuitAI* circuit;
};

} // namespace circuit

#endif // MODULE_H_
