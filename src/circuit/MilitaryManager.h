/*
 * MilitaryManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef MILITARYMANAGER_H_
#define MILITARYMANAGER_H_

#include "Module.h"

#include <map>
#include <functional>

namespace circuit {

class CMilitaryManager: public virtual IModule {
public:
	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);

private:
	void TestOrder();

	std::map<int, std::function<void (CCircuitUnit* unit)>> finishedHandler;
	std::map<int, std::function<void (CCircuitUnit* unit)>> idleHandler;
};

} // namespace circuit

#endif // MILITARYMANAGER_H_
