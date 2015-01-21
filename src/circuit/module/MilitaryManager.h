/*
 * MilitaryManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef MILITARYMANAGER_H_
#define MILITARYMANAGER_H_

#include "module/UnitModule.h"

namespace circuit {

class CMilitaryManager: public IUnitModule {
public:
	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
//	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
//	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int EnemyEnterLOS(CCircuitUnit* unit);

	virtual void AssignTask(CCircuitUnit* unit);
	virtual void ExecuteTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task, CCircuitUnit* unit = nullptr);
	virtual void OnUnitDamaged(CCircuitUnit* unit);

private:
	void TestOrder();

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
//	Handlers2 damagedHandler;
//	Handlers2 destroyedHandler;

//	struct FighterInfo {
//		bool isTerraforming;
//	};
//	std::map<CCircuitUnit*, FighterInfo> fighterInfos;
};

} // namespace circuit

#endif // MILITARYMANAGER_H_
