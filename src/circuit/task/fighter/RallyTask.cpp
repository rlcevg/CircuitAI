/*
 * RallyTask.cpp
 *
 *  Created on: Jan 18, 2016
 *      Author: rlcevg
 */

#include "task/fighter/RallyTask.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CRallyTask::CRallyTask(ITaskManager* mgr, float maxPower)
		: IFighterTask(mgr, FightType::RALLY, 1.f)
		, maxPower(maxPower)
{
	const AIFloat3& pos = manager->GetCircuit()->GetSetupManager()->GetBasePos();
	position = utils::get_radial_pos(pos, SQUARE_SIZE * 32);
}

CRallyTask::~CRallyTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CRallyTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (units.empty()) {
		return true;
	}
	if (attackPower >= maxPower) {
		return false;
	}
	CCircuitDef* cdef = (*units.begin())->GetCircuitDef();
	if ((cdef->IsAbleToFly() && unit->GetCircuitDef()->IsAbleToFly()) ||
		(cdef->IsAmphibious() && unit->GetCircuitDef()->IsAmphibious()) ||
		(cdef->IsLander() && unit->GetCircuitDef()->IsLander()) ||
		(cdef->IsFloater() && unit->GetCircuitDef()->IsFloater()))
	{
		return true;
	}
	return false;
}

void CRallyTask::Execute(CCircuitUnit* unit)
{
	if (attackPower < maxPower) {
		CCircuitAI* circuit = manager->GetCircuit();
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		AIFloat3 pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), position, 300.0f, UNIT_COMMAND_BUILD_NO_FACING);

		TRY_UNIT(circuit, unit,
			unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
			unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {NO_SPEED_LIMIT});
		)
		return;
	}

	IFighterTask* task = static_cast<CMilitaryManager*>(manager)->EnqueueTask(IFighterTask::FightType::ATTACK);
	decltype(units) tmpUnits = units;
	for (CCircuitUnit* ass : tmpUnits) {
		manager->AssignTask(ass, task);
	}
	manager->DoneTask(this);
}

} // namespace circuit
