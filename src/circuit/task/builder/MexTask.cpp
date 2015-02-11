/*
 * MexTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/MexTask.h"
#include "task/TaskManager.h"
#include "unit/CircuitUnit.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"
#include "resource/ResourceManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "OOAICallback.h"
#include "UnitDef.h"
#include "Unit.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CBMexTask::CBMexTask(ITaskManager* mgr, Priority priority,
					 UnitDef* buildDef, const AIFloat3& position,
					 float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::MEX, cost, timeout)
{
}

CBMexTask::~CBMexTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBMexTask::Execute(CCircuitUnit* unit)
{
	Unit* u = unit->GetUnit();

	std::vector<float> params;
	params.push_back(static_cast<float>(priority));
	u->ExecuteCustomCommand(CMD_PRIORITY, params);

	if (target != nullptr) {
		Unit* tu = target->GetUnit();
		u->Build(target->GetDef(), tu->GetPos(), tu->GetBuildingFacing(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
		return;
	}
	CCircuitAI* circuit = manager->GetCircuit();
	if (buildPos != -RgtVector) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildDef, buildPos, facing)) {
			u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
			return;
		} else {
			circuit->GetMetalManager()->SetOpenSpot(buildPos, true);
		}
	}

	buildPos = circuit->GetEconomyManager()->FindBuildPos(unit);
	if (buildPos != -RgtVector) {
		circuit->GetMetalManager()->SetOpenSpot(buildPos, false);
		u->Build(buildDef, buildPos, facing, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 60);
	} else {
		// Fallback to Guard/Assist/Patrol
		manager->FallbackTask(unit);
	}
}

void CBMexTask::Finish()
{
	CCircuitAI* circuit = manager->GetCircuit();
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, circuit->GetUnitDefByName("corrl"), buildPos, IBuilderTask::BuildType::DEFENCE);

	circuit->GetEconomyManager()->UpdateMetalTasks(buildPos);
}

void CBMexTask::OnUnitIdle(CCircuitUnit* unit)
{
	/*
	 * Check if unit is idle because of enemy mex ahead and build turret if so.
	 */
	CCircuitAI* circuit = manager->GetCircuit();
	UnitDef* def = circuit->GetUnitDefByName("corllt");
	float range = def->GetMaxWeaponRange();
	float testRange = range + 200;
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	if (buildPos.SqDistance2D(pos) < testRange * testRange) {
		int mexDefId = circuit->GetEconomyManager()->GetMexDef()->GetUnitDefId();
		// TODO: Use internal CCircuitAI::GetEnemyUnits?
		std::vector<Unit*> enemies = circuit->GetCallback()->GetEnemyUnitsIn(buildPos, 1);
		bool blocked = false;
		for (auto enemy : enemies) {
			UnitDef* def = enemy->GetDef();
			int enemyDefId = def->GetUnitDefId();
			delete def;
			if (enemyDefId == mexDefId) {
				blocked = true;
				break;
			}
		}
		utils::free_clear(enemies);
		if (blocked) {
			CBuilderManager* builderManager = circuit->GetBuilderManager();
			IBuilderTask* task = nullptr;
			float qdist = 200 * 200;
			// TODO: Push tasks into bgi::rtree
			for (auto t : builderManager->GetTasks(IBuilderTask::BuildType::DEFENCE)) {
				if (pos.SqDistance2D(t->GetPos()) < qdist) {
					task = t;
					break;
				}
			}
			if (task == nullptr) {
				AIFloat3 newPos = buildPos - (buildPos - pos).Normalize2D() * range * 0.8;
				task = builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, def, newPos, IBuilderTask::BuildType::DEFENCE);
			}
			manager->AssignTask(unit, task);
			return;
		}
	}

	IBuilderTask::OnUnitIdle(unit);
}

} // namespace circuit
