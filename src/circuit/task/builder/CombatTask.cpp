/*
 * MilitaryTask.cpp
 *
 *  Created on: Apr 10, 2020
 *      Author: rlcevg
 */

#include "task/builder/CombatTask.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;
using namespace terrain;

CCombatTask::CCombatTask(IUnitModule* mgr, float powerMod)
		: IFighterTask(mgr, FightType::DEFEND, powerMod)
{
}

CCombatTask::~CCombatTask()
{
}

void CCombatTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CCombatTask::Start(CCircuitUnit* unit)
{
	Execute(unit);
}

void CCombatTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	if ((++updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC)) {
		lastTouched = frame;
		decltype(units) tmpUnits = units;
		for (CCircuitUnit* unit : tmpUnits) {
			Execute(unit);
		}
	}
}

void CCombatTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	SetTarget(FindTarget(unit, pos));

	if (GetTarget() == nullptr) {
		RemoveAssignee(unit);
		return;
	}

	if (unit->Blocker() != nullptr) {
		return;  // Do not interrupt current action
	}

	const AIFloat3& position = GetTarget()->GetPos();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float range = std::max(unit->GetUnit()->GetMaxRange(), /*unit->IsUnderWater(frame) ? cdef->GetSonarRadius() : */cdef->GetLosRadius());
	if (position.SqDistance2D(pos) < range) {
		unit->Attack(GetTarget(), GetTarget()->GetUnit()->IsCloaked(), frame + FRAMES_PER_SEC * 60);
	} else {
		const AIFloat3 velLead = GetTarget()->GetVel() * FRAMES_PER_SEC * 3;
		const AIFloat3 lead = velLead.SqLength2D() < SQUARE(300.f)
				? velLead
				: AIFloat3(AIFloat3(GetTarget()->GetVel()).Normalize2D() * 300.f);
		AIFloat3 leadPos = position + lead;
		CTerrainManager::CorrectPosition(leadPos);
		TRY_UNIT(circuit, unit,
			unit->CmdMoveTo(leadPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
	}
}

CEnemyInfo* CCombatTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos)
{
	return manager->GetCircuit()->GetMilitaryManager()->FindBCombatTarget(unit, pos, powerMod, false);
}

} // namespace circuit
