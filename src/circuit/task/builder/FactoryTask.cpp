/*
 * FactoryTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/FactoryTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

static int opposite[] = {
	UNIT_FACING_NORTH,
	UNIT_FACING_WEST,
	UNIT_FACING_SOUTH,
	UNIT_FACING_EAST
};

CBFactoryTask::CBFactoryTask(ITaskManager* mgr, Priority priority,
							 CCircuitDef* buildDef, const AIFloat3& position,
							 float cost, float shake, bool isPlop, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::FACTORY, cost, shake, timeout)
		, isPlop(isPlop)
{
	manager->GetCircuit()->GetFactoryManager()->AddFactory(buildDef);
}

CBFactoryTask::~CBFactoryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBFactoryTask::Update()
{
	if (!isPlop) {
		IBuilderTask::Update();
	}
}

void CBFactoryTask::Cancel()
{
	IBuilderTask::Cancel();

	if (target == nullptr) {
		manager->GetCircuit()->GetFactoryManager()->DelFactory(buildDef);
	}
}

void CBFactoryTask::FindBuildSite(CCircuitUnit* builder, const AIFloat3& pos, float searchRadius)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();

//	facing = UNIT_COMMAND_BUILD_NO_FACING;
	float terWidth = terrainManager->GetTerrainWidth();
	float terHeight = terrainManager->GetTerrainHeight();
	if (math::fabs(terWidth - 2 * pos.x) > math::fabs(terHeight - 2 * pos.z)) {
		facing = (2 * pos.x > terWidth) ? UNIT_FACING_WEST : UNIT_FACING_EAST;
	} else {
		facing = (2 * pos.z > terHeight) ? UNIT_FACING_NORTH : UNIT_FACING_SOUTH;
	}

	CTerrainManager::TerrainPredicate predicate = [terrainManager, builder](const AIFloat3& p) {
		return terrainManager->CanBuildAtSafe(builder, p);
	};
	Map* map = circuit->GetMap();
	auto checkFacing = [this, map, terrainManager, &predicate, &pos, searchRadius]() {
		buildPos = terrainManager->FindBuildSite(buildDef, pos, searchRadius, facing, predicate);
		if (!utils::is_valid(buildPos)) {
			return false;
		}

		// decides if a factory should face the opposite direction due to bad terrain
		AIFloat3 posOffset = buildPos;
		const float size = DEFAULT_SLACK;
		switch (facing) {
			default:
			case UNIT_FACING_SOUTH: {  // z++
				posOffset.z += size;
			} break;
			case UNIT_FACING_EAST: {  // x++
				posOffset.x += size;
			} break;
			case UNIT_FACING_NORTH: {  // z--
				posOffset.z -= size;
			} break;
			case UNIT_FACING_WEST: {  // x--
				posOffset.x -= size;
			} break;
		}
		return map->IsPossibleToBuildAt(buildDef->GetUnitDef(), posOffset, facing);
	};

	if (checkFacing()) {
		return;
	}
	facing = opposite[facing];
	if (checkFacing()) {
		return;
	}
	++facing %= 4;
	if (checkFacing()) {
		return;
	}
	facing = opposite[facing];
	checkFacing();
}

} // namespace circuit
