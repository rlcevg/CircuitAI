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
//	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
//	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

private:
	void TestOrder();

	using Handlers1 = std::map<int, std::function<void (CCircuitUnit* unit)>>;
	using Handlers2 = std::map<int, std::function<void (CCircuitUnit* unit, CCircuitUnit* attacker)>>;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
//	Handlers2 damagedHandler;
//	Handlers2 destroyedHandler;

//	struct FighterInfo {
//		bool isTerraforming;
//	};
//	std::map<CCircuitUnit*, FighterInfo> fighterInfo;
};

} // namespace circuit

#endif // MILITARYMANAGER_H_
