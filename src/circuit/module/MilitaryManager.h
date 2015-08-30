/*
 * MilitaryManager.h
 *
 *  Created on: Sep 5, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
#define SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_

#include "module/UnitModule.h"

#include "AIFloat3.h"

#include <vector>
#include <set>

namespace circuit {

class CCircuitDef;

class CMilitaryManager: public IUnitModule {
public:
	struct SDefPoint {
		springai::AIFloat3 position;
		float cost;
	};
	using DefPoints = std::vector<SDefPoint>;
	struct SClusterInfo {
		DefPoints defPoints;
	};

public:
	CMilitaryManager(CCircuitAI* circuit);
	virtual ~CMilitaryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	IUnitTask* EnqueueTask();
private:
	void DequeueTask(IUnitTask* task, bool done = false);

public:
	virtual void AssignTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void SpecialCleanUp(CCircuitUnit* unit);
	virtual void SpecialProcess(CCircuitUnit* unit);
	virtual void FallbackTask(CCircuitUnit* unit);

	std::vector<SDefPoint>& GetDefPoints(int index) { return clusterInfos[index].defPoints; }
	SDefPoint* GetDefPoint(const springai::AIFloat3& pos, float cost);
//	const std::vector<SClusterInfo>& GetClusterInfos() const;

private:
	void Init();

	void UpdateIdle();
	void UpdateRetreat();
	void UpdateFight();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 damagedHandler;
	Handlers2 destroyedHandler;

	std::set<IUnitTask*> fighterTasks;  // owner
	std::set<IUnitTask*> updateTasks;
	std::set<IUnitTask*> deleteTasks;
	unsigned int updateSlice;

//	struct FighterInfo {
//		bool isTerraforming;
//	};
//	std::map<CCircuitUnit*, FighterInfo> fighterInfos;

	std::vector<SClusterInfo> clusterInfos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_MILITARYMANAGER_H_
