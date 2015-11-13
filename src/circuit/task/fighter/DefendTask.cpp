/*
 * DefendTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/DefendTask.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "util/utils.h"
#include "CircuitAI.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CDefendTask::CDefendTask(ITaskManager* mgr, float maxPower)
		: IFighterTask(mgr, FightType::DEFEND)
		, maxPower(maxPower)
{
	AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
	position = manager->GetCircuit()->GetSetupManager()->GetBasePos() + offset.Normalize2D() * SQUARE_SIZE * 32;
}

CDefendTask::~CDefendTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CDefendTask::CanAssignTo(CCircuitUnit* unit)
{
	return attackPower < maxPower;
}

void CDefendTask::Execute(CCircuitUnit* unit)
{
	if (attackPower < maxPower) {
		unit->GetUnit()->MoveTo(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, manager->GetCircuit()->GetLastFrame() + FRAMES_PER_SEC * 300);
		return;
	}
	// NOTE: Atm DefendTask is actually GatheringTask, does nothing more
	IFighterTask* task = static_cast<CMilitaryManager*>(manager)->EnqueueTask(IFighterTask::FightType::ATTACK);
	decltype(units) tmpUnits = units;
	for (CCircuitUnit* ass : tmpUnits) {
		manager->AssignTask(ass, task);
	}
	manager->DoneTask(this);
}

} // namespace circuit
