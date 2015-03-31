/*
 * FactoryManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
#define SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_

#include "task/RecruitTask.h"
#include "task/builder/BuilderTask.h"
#include "module/UnitModule.h"

#include <map>
#include <vector>

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
							  CRecruitTask::BuildType type,
							  float radius);
	IBuilderTask* EnqueueAssist(IBuilderTask::Priority priority,
								const springai::AIFloat3& position,
								IBuilderTask::BuildType type,
								float radius);
private:
	void DequeueTask(IUnitTask* task, bool done = false);

public:
	virtual void AssignTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void SpecialCleanUp(CCircuitUnit* unit);
	virtual void SpecialProcess(CCircuitUnit* unit);
	virtual void FallbackTask(CCircuitUnit* unit);

	float GetFactoryPower();
	bool CanEnqueueTask();
	const std::set<CRecruitTask*>& GetTasks() const;
	CCircuitUnit* NeedUpgrade();
	CCircuitUnit* GetRandomFactory();

	springai::UnitDef* GetAssistDef() const;
	CCircuitUnit* GetClosestHaven(CCircuitUnit* unit) const;
	std::vector<CCircuitUnit*> GetHavensAt(const springai::AIFloat3& pos) const;

private:
	void Watchdog();
	void UpdateIdle();

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, CRecruitTask*> unfinishedUnits;
	std::set<CRecruitTask*> factoryTasks;  // owner
	float factoryPower;
	std::set<IUnitTask*> deleteTasks;

	std::map<CCircuitUnit*, std::set<CCircuitUnit*>> factories;  // factory 1:n nanos
	springai::UnitDef* assistDef;

	std::set<CCircuitUnit*> havens;
	std::set<IBuilderTask*> assistTasks;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
