/*
 * TerraformTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/TerraformTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "Map.h"

namespace circuit {

using namespace springai;

CBTerraformTask::CBTerraformTask(ITaskManager* mgr, Priority priority, CCircuitUnit* target, float cost, int timeout)
		: IBuilderTask(mgr, priority, target->GetCircuitDef(), target->GetPos(mgr->GetCircuit()->GetLastFrame()),
					   Type::BUILDER, BuildType::TERRAFORM, cost, 0.f, timeout)
		, targetId(target->GetId())
{
	facing = target->GetUnit()->GetBuildingFacing();
}

CBTerraformTask::CBTerraformTask(ITaskManager* mgr, Priority priority, const AIFloat3& position, float cost, int timeout)
		: IBuilderTask(mgr, priority, nullptr, position, Type::BUILDER, BuildType::TERRAFORM, cost, 0.f, timeout)
		, targetId(-1)
{
}

CBTerraformTask::~CBTerraformTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBTerraformTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CBTerraformTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (State::ENGAGE == state) {  // !isFirstTry
		return;
	}
	state = State::ENGAGE;  // isFirstTry = false

	/*
	 * Terraform blank position
	 */
	if (targetId == -1) {
		Unit* u = unit->GetUnit();

		if (!utils::is_valid(buildPos)) {
			CTerrainManager* terrainManager = circuit->GetTerrainManager();
			CTerrainManager::TerrainPredicate predicate = [terrainManager, unit](const AIFloat3& p) {
				return terrainManager->CanBuildAt(unit, p);
			};
			CCircuitDef* cdef = circuit->GetBuilderManager()->GetTerraDef();
			buildPos = terrainManager->FindBuildSite(cdef, position, 600.0f, facing, predicate);
		}
		if (!utils::is_valid(buildPos)) {
			manager->DoneTask(this);
			return;
		}

		const float offsetX = 2 * SQUARE_SIZE;
		const float offsetZ = 2 * SQUARE_SIZE;
		std::vector<float> params;
		params.push_back(1.0f);  // 1: terraform_type, 1 == level, 5 == restore
		params.push_back(circuit->GetTeamId());  // 2: teamId
		params.push_back(1.0f);  // 3: terraform type - 0 == Wall, else == Area
		params.push_back(circuit->GetMap()->GetElevationAt(position.x, position.z) + 128.0f);  // 4: terraformHeight
		params.push_back(5.0f);  // 5: number of control points
		params.push_back(1.0f);  // 6: units count?
		params.push_back(0.0f);  // 7: volumeSelection?
		params.push_back(buildPos.x - offsetX);  //  8: i + 0 control point x
		params.push_back(buildPos.z - offsetZ);  //  9: i + 1 control point z
		params.push_back(buildPos.x - offsetX);  //  8: i + 2 control point x
		params.push_back(buildPos.z + offsetZ);  //  9: i + 3 control point z
		params.push_back(buildPos.x + offsetX);  //  8: i + 4 control point x
		params.push_back(buildPos.z + offsetZ);  //  9: i + 5 control point z
		params.push_back(buildPos.x + offsetX);  //  8: i + 6 control point x
		params.push_back(buildPos.z - offsetZ);  //  9: i + 7 control point z
		params.push_back(buildPos.x - offsetX);  //  8: i + 8 control point x
		params.push_back(buildPos.z - offsetZ);  //  9: i + 9 control point z
		params.push_back(unit->GetId());  // 10: i + 10 unitId
		TRY_UNIT(circuit, unit,
			u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
			u->ExecuteCustomCommand(CMD_TERRAFORM_INTERNAL, params);
		)
		return;
	}

	/*
	 * Terraform around unit
	 */
	if (circuit->GetTeamUnit(targetId) == nullptr) {  // is unit still alive?
		manager->AbortTask(this);
		return;
	}

	Unit* u = unit->GetUnit();

	UnitDef* unitDef = buildDef->GetUnitDef();
	const float offsetX = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2 * SQUARE_SIZE + 1 * SQUARE_SIZE + 1;
	const float offsetZ = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2 * SQUARE_SIZE + 1 * SQUARE_SIZE + 1;
	std::vector<float> params;
	params.push_back(1.0f);  // 1: terraform_type, 1 == level
	params.push_back(circuit->GetTeamId());  // 2: teamId
	params.push_back(0.0f);  // 3: terraform type - 0 == Wall, else == Area
	params.push_back(circuit->GetMap()->GetElevationAt(position.x, position.z) + unitDef->GetHeight());  // 4: terraformHeight
	params.push_back(5.0f);  // 5: number of control points
	params.push_back(1.0f);  // 6: units count?
	params.push_back(0.0f);  // 7: volumeSelection?
	params.push_back(position.x - offsetX);  //  8: i + 0 control point x
	params.push_back(position.z - offsetZ);  //  9: i + 1 control point z
	params.push_back(position.x - offsetX);  //  8: i + 2 control point x
	params.push_back(position.z + offsetZ);  //  9: i + 3 control point z
	params.push_back(position.x + offsetX);  //  8: i + 4 control point x
	params.push_back(position.z + offsetZ);  //  9: i + 5 control point z
	params.push_back(position.x + offsetX);  //  8: i + 6 control point x
	params.push_back(position.z - offsetZ);  //  9: i + 7 control point z
	params.push_back(position.x - offsetX);  //  8: i + 8 control point x
	params.push_back(position.z - offsetZ);  //  9: i + 9 control point z
	params.push_back(unit->GetId());  // 10: i + 10 unitId
	TRY_UNIT(circuit, unit,
		u->ExecuteCustomCommand(CMD_PRIORITY, {ClampPriority()});
		u->ExecuteCustomCommand(CMD_TERRAFORM_INTERNAL, params);
	)

	// TODO: Enqueue "move out" action for nearby units
}

void CBTerraformTask::Update()
{
}

void CBTerraformTask::Cancel()
{
	buildPos = -RgtVector;
	IBuilderTask::Cancel();
}

void CBTerraformTask::OnUnitIdle(CCircuitUnit* unit)
{
	float range = unit->GetCircuitDef()->GetBuildDistance() * 2;
	const AIFloat3& pos = unit->GetPos(manager->GetCircuit()->GetLastFrame());
	if (position.SqDistance2D(pos) < range * range) {
		manager->DoneTask(this);
	} else {
		IBuilderTask::OnUnitIdle(unit);
	}
}

} // namespace circuit
