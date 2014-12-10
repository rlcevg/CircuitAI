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
	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
//	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
//	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int EnemyEnterLOS(CCircuitUnit* unit);

private:
	void TestOrder();

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
