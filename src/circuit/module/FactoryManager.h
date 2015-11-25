/*
 * FactoryManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
#define SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_

#include "module/UnitModule.h"
#include "task/RecruitTask.h"
#include "task/builder/BuilderTask.h"

#include <map>
#include <vector>
#include <list>

namespace circuit {

class CEconomyManager;

class CFactoryManager: public IUnitModule {
public:
	CFactoryManager(CCircuitAI* circuit);
	virtual ~CFactoryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder) override;
	virtual int UnitFinished(CCircuitUnit* unit) override;
	virtual int UnitIdle(CCircuitUnit* unit) override;
	virtual int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker) override;

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
	virtual void FallbackTask(CCircuitUnit* unit);

	int GetFactoryCount() const { return factories.size(); }
	float GetFactoryPower() const { return factoryPower; }
	bool CanEnqueueTask() const { return factoryTasks.size() < factories.size() * 2; }
	const std::set<CRecruitTask*>& GetTasks() const { return factoryTasks; }
	CCircuitUnit* NeedUpgrade();
	CCircuitUnit* GetRandomFactory(CCircuitDef* buildDef);

	CCircuitDef* GetAssistDef() const { return assistDef; }
	springai::AIFloat3 GetClosestHaven(CCircuitUnit* unit) const;

	CRecruitTask* UpdateBuildPower(CCircuitUnit* unit);
	CRecruitTask* UpdateFirePower(CCircuitUnit* unit);

private:
	void ReadConfig();

	IUnitTask* CreateFactoryTask(CCircuitUnit* unit);
	IBuilderTask* CreateAssistTask(CCircuitUnit* unit);

	void Watchdog();
	void UpdateIdle();
	void UpdateAssist();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers destroyedHandler;

	std::map<CCircuitUnit*, CRecruitTask*> unfinishedUnits;
	std::set<CRecruitTask*> factoryTasks;  // owner
	float factoryPower;
	std::set<CRecruitTask*> deleteTasks;
	unsigned int updateSlice;

	struct SFactory {
		SFactory(CCircuitUnit* u, const std::set<CCircuitUnit*>& n, unsigned int w, bool h)
			: unit(u)
			, nanos(n)
			, weight(w)
			, hasBuilder(h)
		{}
		CCircuitUnit* unit;
		std::set<CCircuitUnit*> nanos;
		unsigned int weight;
		bool hasBuilder;
	};
	std::list<SFactory> factories;  // facory 1:n nano

	struct SFactoryDef {
		CCircuitDef* builderDef;
		std::vector<CCircuitDef*> buildDefs;
		std::map<unsigned, std::vector<float>> tiers;
		std::vector<float> incomes;
		bool isRequireEnergy;
	};
	std::unordered_map<CCircuitDef::Id, SFactoryDef> factoryDefs;

	CCircuitDef* assistDef;
	std::map<CCircuitUnit*, std::set<CCircuitUnit*>> assists;  // nano 1:n factory
	std::list<springai::AIFloat3> havens;  // position behind factory
	std::set<IBuilderTask*> assistTasks;  // owner
	std::set<IBuilderTask*> updateAssists;
	std::set<IBuilderTask*> deleteAssists;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
