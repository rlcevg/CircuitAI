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
	IUnitTask* EnqueueWait(bool stop, int timeout);
	IBuilderTask* EnqueueReclaim(IBuilderTask::Priority priority,
								 const springai::AIFloat3& position,
								 float radius,
								 int timeout = 0);
	IBuilderTask* EnqueueRepair(IBuilderTask::Priority priority,
								CAllyUnit* target);
private:
	void DequeueTask(IUnitTask* task, bool done = false);

public:
	virtual IUnitTask* MakeTask(CCircuitUnit* unit) override;
	virtual void AbortTask(IUnitTask* task) override;
	virtual void DoneTask(IUnitTask* task) override;
	virtual void FallbackTask(CCircuitUnit* unit) override;

	int GetFactoryCount() const { return factories.size(); }
	float GetFactoryPower() const { return factoryPower; }
	bool CanEnqueueTask() const { return factoryTasks.size() < factories.size() * 2; }
	const std::vector<CRecruitTask*>& GetTasks() const { return factoryTasks; }
	CCircuitUnit* NeedUpgrade();
	CCircuitUnit* GetClosestFactory(springai::AIFloat3 position);
//	CCircuitDef* GetClosestDef(springai::AIFloat3& position, CCircuitDef::RoleType role);

	CCircuitDef* GetAirpadDef() const { return airpadDef; }
	CCircuitDef* GetAssistDef() const { return assistDef; }
	springai::AIFloat3 GetClosestHaven(CCircuitUnit* unit) const;
	springai::AIFloat3 GetClosestHaven(const springai::AIFloat3& position) const;

	CRecruitTask* UpdateBuildPower(CCircuitUnit* unit);
	CRecruitTask* UpdateFirePower(CCircuitUnit* unit);
	bool IsHighPriority(CAllyUnit* unit) const;

	CCircuitDef* GetFactoryToBuild(springai::AIFloat3 position = -RgtVector,
								   bool isStart = false, bool isReset = false);
	void AddFactory(CCircuitDef* cdef);
	void DelFactory(CCircuitDef* cdef);
	CCircuitDef* GetRoleDef(CCircuitDef* facDef, CCircuitDef::RoleType role) const;
	CCircuitDef* GetLandDef(CCircuitDef* facDef) const;
	CCircuitDef* GetWaterDef(CCircuitDef* facDef) const;

private:
	void ReadConfig();
	void Init();
public:
	void Release();

private:
	void EnableFactory(CCircuitUnit* unit);
	void DisableFactory(CCircuitUnit* unit);
	IUnitTask* CreateFactoryTask(CCircuitUnit* unit);
	IUnitTask* CreateAssistTask(CCircuitUnit* unit);

	void Watchdog();
	void UpdateIdle();
	void UpdateFactory();

	Handlers2 createdHandler;
	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	EHandlers destroyedHandler;

	std::map<CAllyUnit*, IBuilderTask*> unfinishedUnits;
	std::vector<CRecruitTask*> factoryTasks;  // order matters
	std::vector<IUnitTask*> updateTasks;  // owner
	unsigned int updateIterator;
	float factoryPower;

	CCircuitDef* airpadDef;
	CCircuitDef* assistDef;
	std::map<CCircuitUnit*, std::set<CCircuitUnit*>> assists;  // nano 1:n factory
	std::vector<springai::AIFloat3> havens;  // position behind factory
	std::map<ICoreUnit::Id, IBuilderTask*> repairedUnits;

	CFactoryData* factoryData;
	struct SFactory {
		SFactory(CCircuitUnit* u, const std::set<CCircuitUnit*>& n, unsigned int w, CCircuitDef* b)
			: unit(u)
			, nanos(n)
			, weight(w)
			, builder(b)
		{}
		CCircuitUnit* unit;
		std::set<CCircuitUnit*> nanos;
		unsigned int weight;
		CCircuitDef* builder;
	};
	std::vector<SFactory> factories;  // facory 1:n nano
	std::set<CCircuitUnit*> validAir;

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
		Tiers airTiers;
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
};

} // namespace circuit

#endif // SRC_CIRCUIT_MODULE_FACTORYMANAGER_H_
