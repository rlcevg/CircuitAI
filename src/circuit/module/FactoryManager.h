/*
 * FactoryManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
#define SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_

#include "task/RecruitTask.h"
#include "module/UnitModule.h"

#include <map>
#include <list>

namespace springai {
	class UnitDef;
}

namespace circuit {

class CFactoryManager: public IUnitModule {
public:
	CFactoryManager(CCircuitAI* circuit);
	virtual ~CFactoryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	CRecruitTask* EnqueueTask(CRecruitTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  CRecruitTask::FacType type,
							  int quantity,
							  float radius);
private:
	void DequeueTask(CRecruitTask* task, bool done = false);

public:
	virtual void AssignTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void SpecialCleanUp(CCircuitUnit* unit);
	virtual void SpecialProcess(CCircuitUnit* unit);
	virtual void FallbackTask(CCircuitUnit* unit);

	float GetFactoryPower();
	bool CanEnqueueTask();
	const std::list<CRecruitTask*>& GetTasks() const;
	CCircuitUnit* NeedUpgrade();
	CCircuitUnit* GetRandomFactory();

	springai::UnitDef* GetAssistDef() const;
	CCircuitUnit* GetClosestHaven(CCircuitUnit* unit) const;

private:
	void Watchdog();
	void UpdateIdle();

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, CRecruitTask*> unfinishedUnits;
	std::map<CRecruitTask*, std::list<CCircuitUnit*>> unfinishedTasks;
	std::list<CRecruitTask*> factoryTasks;  // owner
	float factoryPower;
	std::set<CRecruitTask*> deleteTasks;

	std::map<CCircuitUnit*, std::list<CCircuitUnit*>> factories;
	springai::UnitDef* assistDef;

	std::set<CCircuitUnit*> havens;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
