/*
 * RetreatTask.cpp
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "module/FactoryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "AISCommands.h"

namespace circuit {

using namespace springai;

CRetreatTask::CRetreatTask(ITaskManager* mgr) :
		IUnitTask(mgr, Priority::NORMAL, Type::RETREAT),
		updateSlice(0)
{
}

CRetreatTask::~CRetreatTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CRetreatTask::RemoveAssignee(CCircuitUnit* unit)
{
	updateUnits.erase(unit);

	IUnitTask::RemoveAssignee(unit);
}

void CRetreatTask::Close(bool done)
{
	updateUnits.clear();

	IUnitTask::Close(done);
}

void CRetreatTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	AIFloat3 haven = circuit->GetFactoryManager()->GetClosestHaven(unit);
	const AIFloat3& pos = (haven != -RgtVector) ? haven : circuit->GetSetupManager()->GetStartPos();
	// TODO: push MoveAction into unit? to avoid enemy fire
	unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 1);
}

void CRetreatTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (updateUnits.empty()) {
		updateUnits = units;  // copy units
		updateSlice = updateUnits.size() / TEAM_SLOWUPDATE_RATE;
	}

	auto it = updateUnits.begin();
	unsigned int i = 0;
	while (it != updateUnits.end()) {
		CCircuitUnit* ass = *it;
		it = updateUnits.erase(it);

		Unit* u = ass->GetUnit();
		if (u->GetHealth() >= u->GetMaxHealth() * 0.8f) {
			RemoveAssignee(ass);
		} else {
			CCircuitDef* cdef = ass->GetCircuitDef();
			int reloadFrames = cdef->GetReloadFrames();
			if ((reloadFrames >= 0) && (ass->GetDGunFrame() + reloadFrames < circuit->GetLastFrame())) {
				auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(ass->GetUnit()->GetPos(), cdef->GetDGunRange() * 0.8f));
				if (!enemies.empty()) {
					ass->ManualFire(enemies.front(), circuit->GetLastFrame());
					utils::free_clear(enemies);
				}
			}
		}

		if (++i >= updateSlice) {
			break;
		}
	}
}

void CRetreatTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	AIFloat3 pos = circuit->GetFactoryManager()->GetClosestHaven(unit);
	if (pos == -RgtVector) {
		pos = circuit->GetSetupManager()->GetStartPos();
	}
	float maxDist = 200.0f;
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
