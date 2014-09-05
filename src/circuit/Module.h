/*
 * Module.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef MODULE_H_
#define MODULE_H_

namespace circuit {

class CCircuit;
class CCircuitUnit;

class IModule {
public:
	virtual ~IModule();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);

protected:
	IModule(CCircuit* circuit);

	CCircuit* circuit;
};

} // namespace circuit

#endif // MODULE_H_
