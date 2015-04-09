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

namespace circuit {

class CCircuitDef;

class CFactoryManager: public IUnitModule {
public:
	CFactoryManager(CCircuitAI* circuit);
	virtual ~CFactoryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	CRecruitTask* EnqueueTask(CRecruitTask::Priority priority,
							  CCircuitDef* buildDef,
							  const springai::AIFloat3& position,
							  CRecruitTask::BuildType type,
							  float radius);
	IBuilderTask* EnqueueReclaim(IBuilderTask::Priority priority,
								 const springai::AIFloat3& position,
								 float radius,
								 int timeout = 0);
	IBuilderTask* EnqueueRepair(IBuilderTask::Priority priority,
								CCircuitUnit* target);
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

	CCircuitDef* GetAssistDef() const;
	CCircuitUnit* GetClosestHaven(CCircuitUnit* unit) const;
	std::vector<CCircuitUnit*> GetHavensAt(const springai::AIFloat3& pos) const;

private:
	void Watchdog();
	void UpdateIdle();
	void UpdateAssist();

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, CRecruitTask*> unfinishedUnits;
	std::set<CRecruitTask*> factoryTasks;  // owner
	float factoryPower;
	std::set<CRecruitTask*> deleteTasks;

	std::map<CCircuitUnit*, std::set<CCircuitUnit*>> factories;  // factory 1:n nanos
	CCircuitDef* assistDef;

	std::set<CCircuitUnit*> havens;
	std::set<IBuilderTask*> assistTasks;  // owner
	std::set<IBuilderTask*> updateAssists;
	std::set<IBuilderTask*> deleteAssists;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
