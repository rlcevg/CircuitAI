/*
 * RepairTask.cpp
 *
 *  Created on: Sep 4, 2016
 *      Author: rlcevg
 */

#include "task/common/RepairTask.h"
#include "task/RetreatTask.h"
#include "map/InfluenceMap.h"
#include "module/BuilderManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

IRepairTask::IRepairTask(IUnitModule* mgr, Priority priority, Type type, CAllyUnit* target, int timeout)
		: IBuilderTask(mgr, priority, nullptr, -RgtVector, type, BuildType::REPAIR, {1000.f, 0.f}, 0.f, timeout)
{
	SetRepTarget(target);
}

IRepairTask::IRepairTask(IUnitModule* mgr, Type type)
		: IBuilderTask(mgr, type, BuildType::REPAIR)
		, targetId(-1)
{
}

IRepairTask::~IRepairTask()
{
}

bool IRepairTask::CanAssignTo(CCircuitUnit* unit) const
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitDef* cdef = unit->GetCircuitDef();
	return cdef->IsAbleToRepair() && !unit->IsAttrSolo()
			&& (target != nullptr) && (target != unit) && (cost.metal > buildPower.metal * static_cast<CBuilderManager*>(manager)->GetGoalExecTime())
			&& (circuit->GetInflMap()->GetInfluenceAt(unit->GetPos(circuit->GetLastFrame())) > INFL_EPS);
}

void IRepairTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);

	// Inform repair-target task/unit
	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
	if (repTarget == nullptr) {
		return;
	}
	IUnitTask* task = repTarget->GetTask();
	if ((task == nullptr) || (task->GetType() != IUnitTask::Type::RETREAT)) {
		return;
	}
	CRetreatTask* retTask = static_cast<CRetreatTask*>(task);
	if (retTask->GetRepairer() != unit) {
		return;
	}
	retTask->SetRepairer(nullptr);
	if (!units.empty()) {
		retTask->CheckRepairer(*units.begin());
	}
}

void IRepairTask::Start(CCircuitUnit* unit)
{
	if (targetId == -1) {
		CAllyUnit* repTarget = FindUnitToAssist(unit);
		if (repTarget == nullptr) {
			manager->FallbackTask(unit);
			return;
		}
		cost.metal = repTarget->GetCircuitDef()->GetCostM();
		targetId = repTarget->GetId();
	}
}

void IRepairTask::Finish()
{
//	CCircuitAI* circuit = manager->GetCircuit();
//	CAllyUnit* target = circuit->GetFriendlyUnit(targetId);
//	// FIXME: Replace const 1000.0f with build time?
//	if (target != nullptr) {
//		CCircuitDef* cdef = target->GetCircuitDef();
//		if (!cdef->IsMobile() && !cdef->IsAttacker() && (cdef->GetCost() > 1000.0f)) {
//			circuit->GetBuilderManager()->EnqueueTask(TaskB::Terraform(IBuilderTask::Priority::HIGH, target));
//		}
//	}

	Cancel();
}

void IRepairTask::Cancel()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);
	if (repTarget == nullptr) {
		return;
	}
	IUnitTask* task = repTarget->GetTask();
	if ((task == nullptr) || (task->GetType() != IUnitTask::Type::RETREAT)) {
		return;
	}
	CRetreatTask* retTask = static_cast<CRetreatTask*>(task);
	CCircuitUnit* repairer = retTask->GetRepairer();
	for (CCircuitUnit* unit : units) {
		if (repairer == unit) {
			retTask->SetRepairer(nullptr);
			break;
		}
	}
}

bool IRepairTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* repTarget = (target != nullptr) ? target : circuit->GetFriendlyUnit(targetId);

	if ((repTarget != nullptr) && (repTarget->GetUnit()->GetHealth() < repTarget->GetUnit()->GetMaxHealth())) {
		TRY_UNIT(circuit, unit,
			unit->CmdPriority(ClampPriority());
			unit->CmdRepair(repTarget, UNIT_CMD_OPTION, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		)

		IUnitTask* task = repTarget->GetTask();
		if ((task != nullptr) && (task->GetType() == IUnitTask::Type::RETREAT)) {
			static_cast<CRetreatTask*>(task)->CheckRepairer(unit);
		}
	} else {
		manager->AbortTask(this);
		return false;
	}
	return true;
}

void IRepairTask::SetRepTarget(CAllyUnit* unit)
{
	if (unit != nullptr) {
		CCircuitAI* circuit = manager->GetCircuit();
		target = circuit->GetTeamUnit(unit->GetId());  // can be nullptr, using targetId
		cost.metal = unit->GetCircuitDef()->GetCostM();
		position = buildPos = unit->GetPos(circuit->GetLastFrame());
//		CTerrainManager::CorrectPosition(buildPos);  // position will contain non-corrected value
		targetId = unit->GetId();
		if (unit->GetUnit()->IsBeingBuilt()) {
			buildDef = circuit->GetCircuitDef(unit->GetCircuitDef()->GetId());
		} else {
			savedIncome = {0.f, 0.f};
		}
	} else {
		target = nullptr;
		cost.metal = 1000.f;
		position = buildPos = -RgtVector;
		targetId = -1;
		buildDef = nullptr;
	}
}

CAllyUnit* IRepairTask::FindUnitToAssist(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CAllyUnit* target = nullptr;
	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
	float maxSpeed = unit->GetCircuitDef()->GetSpeed();
	float radius = unit->GetCircuitDef()->GetBuildDistance() + maxSpeed * 30;
	maxSpeed = SQUARE(maxSpeed * 1.5f / FRAMES_PER_SEC);

	circuit->UpdateFriendlyUnits();
	auto& units = circuit->GetCallback()->GetFriendlyUnitsIn(pos, radius);
	for (Unit* u : units) {
		if ((u->GetHealth() < u->GetMaxHealth()) && (u->GetVel().SqLength2D() <= maxSpeed)) {
			target = circuit->GetFriendlyUnit(u);
			if (target != nullptr) {
				break;
			}
		}
	}
	utils::free(units);
	return target;
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, targetId);

bool IRepairTask::Load(std::istream& is)
{
	IBuilderTask::Load(is);
	SERIALIZE(is, read)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | targetId=%i", __PRETTY_FUNCTION__, targetId);
#endif
	return true;
}

void IRepairTask::Save(std::ostream& os) const
{
	IBuilderTask::Save(os);
	SERIALIZE(os, write)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | targetId=%i", __PRETTY_FUNCTION__, targetId);
#endif
}

} // namespace circuit
