/*
 * DefenceTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/DefenceTask.h"
#include "map/InfluenceMap.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "module/MilitaryManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

#include "Feature.h"

namespace circuit {

using namespace springai;

CBDefenceTask::CBDefenceTask(IUnitModule* mgr, Priority priority,
							 CCircuitDef* buildDef, const AIFloat3& position,
							 SResource cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::DEFENCE, cost, shake, timeout)
		, isUrgent(false)
		, normalCostM(cost.metal)
		, defPointId(-1)
{
}

CBDefenceTask::CBDefenceTask(IUnitModule* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::DEFENCE)
		, isUrgent(false)
		, normalCostM(.0f)
		, defPointId(-1)
{
}

CBDefenceTask::~CBDefenceTask()
{
}

bool CBDefenceTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (manager->GetCircuit()->GetFactoryManager()->GetFactoryCount() == 0) {
		return false;
	}
	return IBuilderTask::CanAssignTo(unit);
}

void CBDefenceTask::Update()
{
	IBuilderTask::Update();
	if (units.empty() || (target == nullptr)) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	bool isUnderEnemy = (circuit->GetInflMap()->GetEnemyInflAt(buildPos) > INFL_EPS);
	if (isUrgent == isUnderEnemy) {
		return;
	}
	isUrgent = isUnderEnemy;
	if (isUnderEnemy) {
		priority = IBuilderTask::Priority::HIGH;
		cost.metal *= 8.f;  // attract more builders
	} else {
		priority = IBuilderTask::Priority::NORMAL;
		cost.metal = normalCostM;
	}
	TRY_UNIT(circuit, target,
		target->CmdPriority(ClampPriority());
	)
}

void CBDefenceTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (defPointId >= 0) {
		circuit->GetMilitaryManager()->MarkPorc(target, defPointId);
	}
	// Reclaim turret blockers
	const float radius = 128.0f;  // buildDef->GetMaxRange() * 0.5f;
	if (circuit->GetCallback()->IsFeaturesIn(buildPos, radius)) {
		circuit->GetBuilderManager()->Enqueue(TaskB::Reclaim(IBuilderTask::Priority::HIGH,
				buildPos, .0f, FRAMES_PER_SEC * 60, radius, false));
	}

	IBuilderTask::Finish();
}

void CBDefenceTask::Cancel()
{
	if (defPointId >= 0) {
		manager->GetCircuit()->GetMilitaryManager()->AbortDefence(this, defPointId);
	}

	IBuilderTask::Cancel();
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, isUrgent);		\
	utils::binary_##func(stream, normalCostM);

bool CBDefenceTask::Load(std::istream& is)
{
	IBuilderTask::Load(is);
	SERIALIZE(is, read)
	// TODO: Save DefenceData::defPoints cost, otherwise save-loading defPointId makes no sense
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | isUrgent=%i | normalCostM=%f", __PRETTY_FUNCTION__, isUrgent, normalCostM);
#endif
	return true;
}

void CBDefenceTask::Save(std::ostream& os) const
{
	IBuilderTask::Save(os);
	SERIALIZE(os, write)
#ifdef DEBUG_SAVELOAD
	manager->GetCircuit()->LOG("%s | isUrgent=%i | normalCostM=%f", __PRETTY_FUNCTION__, isUrgent, normalCostM);
#endif
}

} // namespace circuit
