/*
 * FactoryManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
#define SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_

#include "module/UnitModule.h"
#include "task/static/RecruitTask.h"
#include "unit/CircuitUnit.h"

#include <map>

namespace circuit {

class CEconomyManager;
class CFactoryData;

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
							  CRecruitTask::RecruitType type,
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
	virtual IUnitTask* MakeTask(CCircuitUnit* unit);
	virtual void AbortTask(IUnitTask* task);
	virtual void DoneTask(IUnitTask* task);
	virtual void FallbackTask(CCircuitUnit* unit);

	int GetFactoryCount() const { return factories.size(); }
	float GetFactoryPower() const { return factoryPower; }
	bool CanEnqueueTask() const { return factoryTasks.size() < factories.size() * 2; }
	const std::set<CRecruitTask*>& GetTasks() const { return factoryTasks; }
	CCircuitUnit* NeedUpgrade();
	CCircuitUnit* GetRandomFactory(CCircuitDef* buildDef);
	CCircuitUnit* GetClosestFactory(springai::AIFloat3 position);
	CCircuitDef* GetClosestDef(springai::AIFloat3& position, CCircuitDef::RoleType role);

	CCircuitDef* GetAssistDef() const { return assistDef; }
	springai::AIFloat3 GetClosestHaven(CCircuitUnit* unit) const;

	CRecruitTask* UpdateBuildPower(CCircuitUnit* unit);
	CRecruitTask* UpdateFirePower(CCircuitUnit* unit);

	CCircuitDef* GetFactoryToBuild(bool isStart = false);
	void AddFactory(CCircuitDef* cdef);
	void DelFactory(CCircuitDef* cdef);
	CCircuitDef* GetRoleDef(CCircuitDef* facDef, CCircuitDef::RoleType role) const;
	CCircuitDef* GetLandDef(CCircuitDef* facDef) const;
	CCircuitDef* GetWaterDef(CCircuitDef* facDef) const;

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

	std::map<CCircuitUnit*, IBuilderTask*> unfinishedUnits;
	std::set<CRecruitTask*> factoryTasks;  // owner
	float factoryPower;
	std::set<CRecruitTask*> deleteTasks;

	CFactoryData* factoryData;
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
	std::vector<SFactory> factories;  // facory 1:n nano

	struct SFactoryDef {
		using Tiers = std::map<unsigned, std::vector<float>>;

		SFactoryDef()
			: landDef(nullptr)
			, waterDef(nullptr)
			, isRequireEnergy(false)
			, nanoCount(0)
		{}
		CCircuitDef* GetRoleDef(CCircuitDef::RoleType role) const {
			return roleDefs[static_cast<CCircuitDef::RoleT>(role)];
		}

		std::vector<CCircuitDef*> roleDefs;  // cheapest role def
		std::vector<CCircuitDef*> buildDefs;
		Tiers landTiers;
		Tiers waterTiers;
		CCircuitDef* landDef;
		CCircuitDef* waterDef;
		std::vector<float> incomes;
		bool isRequireEnergy;
		unsigned int nanoCount;
	};
	std::unordered_map<CCircuitDef::Id, SFactoryDef> factoryDefs;
	float bpRatio;
	float reWeight;

	CCircuitDef* assistDef;
	std::map<CCircuitUnit*, std::set<CCircuitUnit*>> assists;  // nano 1:n factory
	std::vector<springai::AIFloat3> havens;  // position behind factory
	std::map<CCircuitUnit::Id, IBuilderTask*> repairedUnits;
	std::vector<IBuilderTask*> assistTasks;  // owner
	unsigned int assistIterator;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
