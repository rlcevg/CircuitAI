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
		: IFighterTask(mgr, FightType::RALLY)
		, maxPower(maxPower)
{
	AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
	position = manager->GetCircuit()->GetSetupManager()->GetBasePos() + offset.Normalize2D() * SQUARE_SIZE * 32;
}

CRallyTask::~CRallyTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CRallyTask::CanAssignTo(CCircuitUnit* unit)
{
	return attackPower < maxPower;
}

void CRallyTask::Execute(CCircuitUnit* unit)
{
	if (attackPower < maxPower) {
		CCircuitAI* circuit = manager->GetCircuit();
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		AIFloat3 pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), position, 300.0f, UNIT_COMMAND_BUILD_NO_FACING);
		unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
		unit->GetUnit()->SetWantedMaxSpeed(MAX_SPEED);
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
