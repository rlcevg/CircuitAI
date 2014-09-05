/*
 * MilitaryManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef MILITARYMANAGER_H_
#define MILITARYMANAGER_H_

#include "Module.h"

namespace circuit {

class CMilitaryManager: public virtual IModule {
public:
	CMilitaryManager(CCircuit* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);

private:
	void TestOrder();
};

} // namespace circuit

#endif // MILITARYMANAGER_H_
