/*
 * RetreatTask.cpp
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#include "task/RetreatTask.h"
#include "unit/CircuitUnit.h"
#include "unit/UnitManager.h"
#include "module/FactoryManager.h"
#include "static/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "AIFloat3.h"
#include "UnitDef.h"
#include "Unit.h"

namespace circuit {

using namespace springai;

CRetreatTask::CRetreatTask() :
		IUnitTask(Priority::NORMAL, Type::RETREAT)
{
}

CRetreatTask::~CRetreatTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CRetreatTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitAI* circuit = unit->GetManager()->GetCircuit();
	CCircuitUnit* haven = circuit->GetFactoryManager()->GetClosestHaven(unit);
	const AIFloat3& pos = (haven != nullptr) ? haven->GetUnit()->GetPos() : circuit->GetSetupManager()->GetStartPos();
	// TODO: push MoveAction into unit? to avoid enemy fire
	unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 1);
}

void CRetreatTask::Update(CCircuitAI* circuit)
{
	auto assignees = units;
	for (auto ass : assignees) {
		Unit* u = ass->GetUnit();
		if (u->GetHealth() >= u->GetMaxHealth() * 0.8) {
			RemoveAssignee(ass);
		}
	}
}

void CRetreatTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = unit->GetManager()->GetCircuit();
	CCircuitUnit* haven = circuit->GetFactoryManager()->GetClosestHaven(unit);
	AIFloat3 pos;
	float maxDist;
	if (haven != nullptr) {
		pos = haven->GetUnit()->GetPos();
		maxDist = haven->GetDef()->GetBuildDistance() * 0.5;
	} else {
		pos = circuit->GetSetupManager()->GetStartPos();
		maxDist = 200;
	}
	Unit* u = unit->GetUnit();
	if (u->GetPos().SqDistance2D(pos) > maxDist * maxDist) {
		// TODO: push MoveAction into unit? to avoid enemy fire
		u->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 1);
		// TODO: Add fail counter?
	} else {
		std::vector<float> params;
		params.push_back(0.0f);
		u->ExecuteCustomCommand(CMD_PRIORITY, params);

		AIFloat3 pos = u->GetPos();
		const float size = SQUARE_SIZE * 10;
		CTerrainManager* terrain = circuit->GetTerrainManager();
		pos.x += (pos.x > terrain->GetTerrainWidth() / 2) ? -size : size;
		pos.z += (pos.z > terrain->GetTerrainHeight() / 2) ? -size : size;
		u->PatrolTo(pos);
	}
}

void CRetreatTask::OnUnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	//TODO: Rebuild retreat path?
}

void CRetreatTask::OnUnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
