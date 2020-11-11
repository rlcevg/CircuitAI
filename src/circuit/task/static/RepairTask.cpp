/*
 * RepairTask.cpp
 *
 *  Created on: Mar 30, 2015
 *      Author: rlcevg
 */

#include "task/static/RepairTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "module/FactoryManager.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

#include "AISCommands.h"
#include "Feature.h"

namespace circuit {

using namespace springai;

CSRepairTask::CSRepairTask(ITaskManager* mgr, Priority priority, CAllyUnit* target, int timeout)
		: IRepairTask(mgr, priority, Type::FACTORY, target, timeout)
{
}

CSRepairTask::~CSRepairTask()
{
}

void CSRepairTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	ShowAssignee(unit);
	if (!utils::is_valid(position)) {
		position = unit->GetPos(circuit->GetLastFrame());
	}

	if (unit->HasDGun()) {
		unit->PushDGunAct(new CDGunAction(unit, unit->GetDGunRange()));
	}
}

void CSRepairTask::Start(CCircuitUnit* unit)
{
	IRepairTask::Start(unit);
	if (targetId != -1) {
		Execute(unit);
	}
}

void CSRepairTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CEconomyManager* economyMgr = circuit->GetEconomyManager();

	const bool isEnergyEmpty = economyMgr->IsEnergyEmpty();
	for (CCircuitUnit* unit : units) {
		TRY_UNIT(circuit, unit,
			unit->CmdWait(isEnergyEmpty);
		)
	}
	if (isEnergyEmpty) {
		return;
	}

	if (economyMgr->GetAvgMetalIncome() < savedIncome * 0.6f) {
		manager->AbortTask(this);
	} else if ((++updCount % 4 == 0) && !units.empty()) {
		const float radius = (*units.begin())->GetCircuitDef()->GetBuildDistance();
		CAllyUnit* repTarget = circuit->GetFriendlyUnit(targetId);
		if ((repTarget == nullptr)
			|| (position.SqDistance2D(repTarget->GetPos(circuit->GetLastFrame())) > SQUARE(radius * 0.9f)))
		{
			manager->AbortTask(this);
			return;
		}

		IBuilderTask* task = nullptr;
		if (repTarget->GetUnit()->IsBeingBuilt()) {
			CFactoryManager* factoryMgr = circuit->GetFactoryManager();
			if (economyMgr->IsMetalEmpty() && !factoryMgr->IsHighPriority(repTarget)) {
				// Check for damaged units
				CBuilderManager* builderMgr = circuit->GetBuilderManager();
				circuit->UpdateFriendlyUnits();
				auto us = circuit->GetCallback()->GetFriendlyUnitsIn(position, radius * 0.9f);
				for (Unit* u : us) {
					CAllyUnit* candUnit = circuit->GetFriendlyUnit(u);
					if ((candUnit == nullptr) || builderMgr->IsReclaimUnit(candUnit)
						|| candUnit->GetCircuitDef()->IsMex())  // FIXME: BA, should be IsT1Mex()
					{
						continue;
					}
					if (!u->IsBeingBuilt() && (u->GetHealth() < u->GetMaxHealth())) {
						task = factoryMgr->EnqueueRepair(IBuilderTask::Priority::NORMAL, candUnit);
						break;
					}
				}
				utils::free_clear(us);
				if (task == nullptr) {
					// Reclaim task
					if (circuit->GetCallback()->IsFeaturesIn(position, radius) && !builderMgr->IsResurrect(position, radius)) {
						task = factoryMgr->EnqueueReclaim(IBuilderTask::Priority::NORMAL, position, radius);
					}
				}
			}
		} else if (economyMgr->IsMetalFull()) {
			// Check for units under construction
			CFactoryManager* factoryMgr = circuit->GetFactoryManager();
			CBuilderManager* builderMgr = circuit->GetBuilderManager();
			const float maxCost = MAX_BUILD_SEC * economyMgr->GetAvgMetalIncome() * economyMgr->GetEcoFactor();
			circuit->UpdateFriendlyUnits();
			auto us = circuit->GetCallback()->GetFriendlyUnitsIn(position, radius * 0.9f);
			for (Unit* u : us) {
				CAllyUnit* candUnit = circuit->GetFriendlyUnit(u);
				if ((candUnit == nullptr) || builderMgr->IsReclaimUnit(candUnit)
					|| candUnit->GetCircuitDef()->IsMex())  // FIXME: BA, should be IsT1Mex()
				{
					continue;
				}
				bool isHighPrio = factoryMgr->IsHighPriority(candUnit);
				if (u->IsBeingBuilt() && ((candUnit->GetCircuitDef()->GetBuildTime() < maxCost) || isHighPrio)) {
					IBuilderTask::Priority priority = isHighPrio ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
					task = factoryMgr->EnqueueRepair(priority, candUnit);
					break;
				}
			}
			utils::free_clear(us);
		}

		if (task != nullptr) {
			decltype(units) tmpUnits = units;
			for (CCircuitUnit* unit : tmpUnits) {
				manager->AssignTask(unit, task);
			}
			manager->AbortTask(this);
		}
	}
}

void CSRepairTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	for (CCircuitUnit* unit : units) {
		TRY_UNIT(circuit, unit,
			unit->CmdPriority(0);
			unit->GetUnit()->PatrolTo(position, UNIT_COMMAND_OPTION_SHIFT_KEY);
		)
	}

	IRepairTask::Finish();
}

void CSRepairTask::OnUnitIdle(CCircuitUnit* unit)
{
	manager->DoneTask(this);
}

void CSRepairTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	// TODO: Terraform attacker into dust
}

} // namespace circuit
