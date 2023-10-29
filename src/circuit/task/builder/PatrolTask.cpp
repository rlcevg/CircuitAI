/*
 * PatrolTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/PatrolTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
#include "unit/CircuitUnit.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "Map.h"

namespace circuit {

using namespace springai;

CBPatrolTask::CBPatrolTask(ITaskManager* mgr, Priority priority,
						   const AIFloat3& position,
						   int timeout)
		: IBuilderTask(mgr, priority, nullptr, position, Type::BUILDER, BuildType::PATROL, {0.f, 0.f}, 0.f, timeout)
{
}

CBPatrolTask::~CBPatrolTask()
{
}

void CBPatrolTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	ShowAssignee(unit);
	if (!utils::is_valid(position)) {
		position = unit->GetPos(circuit->GetLastFrame());
	}

	if (unit->HasDGun()) {
		const float range = std::max(unit->GetDGunRange(), unit->GetCircuitDef()->GetLosRadius());
		unit->PushDGunAct(new CDGunAction(unit, range));
	}

	lastTouched = circuit->GetLastFrame();
}

void CBPatrolTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);

	manager->AbortTask(this);
}

void CBPatrolTask::Start(CCircuitUnit* unit)
{
	Execute(unit);
}

void CBPatrolTask::Update()
{
}

void CBPatrolTask::Finish()
{
}

void CBPatrolTask::Cancel()
{
}

bool CBPatrolTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	const float size = SQUARE_SIZE * 100;
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	AIFloat3 pos = position;
	pos.x += (pos.x > terrainMgr->GetTerrainWidth() / 2) ? -size : size;
	pos.z += (pos.z > terrainMgr->GetTerrainHeight() / 2) ? -size : size;
	CTerrainManager::CorrectPosition(pos);

	TRY_UNIT(circuit, unit,
		unit->CmdPriority(0);
		unit->CmdPatrolTo(pos);
	)
	return true;
}

} // namespace circuit
