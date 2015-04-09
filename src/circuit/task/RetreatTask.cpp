/*
 * RetreatTask.cpp
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "unit/CircuitUnit.h"
#include "module/FactoryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "AIFloat3.h"
#include "UnitDef.h"

namespace circuit {

using namespace springai;

CRetreatTask::CRetreatTask(ITaskManager* mgr) :
		IUnitTask(mgr, Priority::NORMAL, Type::RETREAT)
{
}

CRetreatTask::~CRetreatTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CRetreatTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitUnit* haven = circuit->GetFactoryManager()->GetClosestHaven(unit);
	const AIFloat3& pos = (haven != nullptr) ? haven->GetUnit()->GetPos() : circuit->GetSetupManager()->GetStartPos();
	// TODO: push MoveAction into unit? to avoid enemy fire
	unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 1);
}

void CRetreatTask::Update()
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
	CCircuitAI* circuit = manager->GetCircuit();
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
