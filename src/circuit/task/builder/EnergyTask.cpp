/*
 * EnergyTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/EnergyTask.h"
#include "module/UnitModule.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBEnergyTask::CBEnergyTask(IUnitModule* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   SResource cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::ENERGY, cost, shake, timeout)
		, isStalling(false)
{
}

CBEnergyTask::CBEnergyTask(IUnitModule* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::ENERGY)
		, isStalling(false)
{
}

CBEnergyTask::~CBEnergyTask()
{
}

bool CBEnergyTask::CanAssignTo(CCircuitUnit* unit) const
{
	// is extra buildpower required?
	if (!IBuilderTask::CanAssignTo(unit)) {
		return false;
	}
	const int frame = manager->GetCircuit()->GetLastFrame();
	return (cost.metal > 200.0f) || !unit->GetCircuitDef()->IsRoleComm()
		|| (GetPosition().SqDistance2D(unit->GetPos(frame)) < SQUARE((unit->GetCircuitDef()->GetBuildDistance() + 128.f)))
		|| !utils::is_valid(GetBuildPos());
}

void CBEnergyTask::Update()
{
	IBuilderTask::Update();
	if (units.empty() || (target == nullptr)) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	bool isEnergyStalling = circuit->GetEconomyManager()->IsEnergyStalling();
	if (isStalling == isEnergyStalling) {
		return;
	}
	isStalling = isEnergyStalling;
	priority = isEnergyStalling ? IBuilderTask::Priority::HIGH : IBuilderTask::Priority::NORMAL;
	TRY_UNIT(circuit, target,
		target->CmdPriority(ClampPriority());
	)
}

void CBEnergyTask::Finish()
{
	CEconomyManager* economyMgr = manager->GetCircuit()->GetEconomyManager();
	economyMgr->ClearEnergyRequired();
	if (economyMgr->IsEnergyStalling()) {
		economyMgr->UpdateEnergyTasks(buildPos, initiator);
	}

	IBuilderTask::Finish();
}

void CBEnergyTask::Cancel()
{
	manager->GetCircuit()->GetEconomyManager()->ClearEnergyRequired();

	IBuilderTask::Cancel();
}

bool CBEnergyTask::Reevaluate(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	if ((target == nullptr) && (cost.metal < 200.0f) && (units.size() == 1) && unit->GetCircuitDef()->IsRoleComm()) {
		const AIFloat3& newPos = unit->GetPos(frame);
		if (GetTaskPos().SqDistance2D(newPos) > SQUARE(unit->GetCircuitDef()->GetBuildDistance() + 128.f)) {
			const float buildTime = buildDef->GetBuildTime() / unit->GetCircuitDef()->GetWorkerTime();
//			const float miRequire = buildDef->GetCostM() / buildTime;
			if ((buildTime < 10)/* && (miRequire < circuit->GetEconomyManager()->GetAvgMetalIncome())*/) {
				if (circuit->GetTerrainManager()->CanBeBuiltAt(buildDef, newPos)) {
					SetBuildPos(-RgtVector);
					SetTaskPos(newPos);
				} else {
					manager->AbortTask(this);
					circuit->GetEconomyManager()->UpdateEnergyTasks(newPos, unit);
					return false;
				}
			}
		}
	}
	return IBuilderTask::Reevaluate(unit);
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, isStalling);

bool CBEnergyTask::Load(std::istream& is)
{
	IBuilderTask::Load(is);
	SERIALIZE(is, read)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | isStalling=%i", __PRETTY_FUNCTION__, isStalling);
#endif
	return true;
}

void CBEnergyTask::Save(std::ostream& os) const
{
	IBuilderTask::Save(os);
	SERIALIZE(os, write)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | isStalling=%i", __PRETTY_FUNCTION__, isStalling);
#endif
}

} // namespace circuit
