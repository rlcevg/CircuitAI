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
						   float cost, int timeout)
		: IBuilderTask(mgr, priority, nullptr, position, Type::BUILDER, BuildType::PATROL, cost, 0.f, timeout)
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
		unit->PushDGunAct(new CDGunAction(unit, unit->GetDGunRange()));
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

void CBPatrolTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(0);

		const float size = SQUARE_SIZE * 100;
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		AIFloat3 pos = position;
		pos.x += (pos.x > terrainManager->GetTerrainWidth() / 2) ? -size : size;
		pos.z += (pos.z > terrainManager->GetTerrainHeight() / 2) ? -size : size;
		unit->GetUnit()->PatrolTo(pos);
	)
}

} // namespace circuit
