/*
 * FactoryTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/FactoryTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

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
							 float cost, bool isShake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, BuildType::FACTORY, cost, isShake, timeout)
{
}

CBFactoryTask::~CBFactoryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

int CBFactoryTask::FindFacing(CCircuitDef* buildDef, const AIFloat3& position)
{
	int facing = IBuilderTask::FindFacing(buildDef, position);

	// decides if a factory should face the opposite direction due to bad terrain
	CTerrainManager* terrainManager = manager->GetCircuit()->GetTerrainManager();
	AIFloat3 posOffset = position;
	const float size = terrainManager->GetConvertStoP();
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
	if (!terrainManager->CanBeBuiltAt(buildDef, posOffset)) {
		facing = opposite[facing];
	}

	return facing;
}

} // namespace circuit
