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
#include "scheduler/Scheduler.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

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
							 CCircuitDef* buildDef, CCircuitDef* reprDef, const AIFloat3& position,
							 float cost, float shake, bool isPlop, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::FACTORY, cost, shake, timeout)
		, reprDef(reprDef)
		, isPlop(isPlop)
{
	manager->GetCircuit()->GetFactoryManager()->AddFactory(buildDef);
}

CBFactoryTask::CBFactoryTask(ITaskManager* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::FACTORY)
		, reprDef(nullptr)
		, isPlop(false)
{
}

CBFactoryTask::~CBFactoryTask()
{
}

void CBFactoryTask::Start(CCircuitUnit* unit)
{
	if (isPlop) {
		Execute(unit);
	} else {
		IBuilderTask::Start(unit);
	}
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

void CBFactoryTask::Activate()
{
	manager->GetCircuit()->GetFactoryManager()->ApplySwitchFrame();
	IBuilderTask::Activate();
}

void CBFactoryTask::FindBuildSite(CCircuitUnit* builder, const AIFloat3& pos, float searchRadius)
{
	FindFacing(pos);

	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CTerrainManager::TerrainPredicate predicate;
	if (reprDef == nullptr) {
		predicate = [terrainMgr, builder](const AIFloat3& p) {
			return terrainMgr->CanReachAtSafe(builder, p, builder->GetCircuitDef()->GetBuildDistance());
		};
	} else {
		CCircuitDef* reprDef = this->reprDef;
		predicate = [terrainMgr, builder, reprDef](const AIFloat3& p) {
			return terrainMgr->CanReachAtSafe(builder, p, builder->GetCircuitDef()->GetBuildDistance())
					&& terrainMgr->CanBeBuiltAt(reprDef, p);
		};
	}
	CMap* map = circuit->GetMap();
	const float testSize = std::max(buildDef->GetDef()->GetXSize(), buildDef->GetDef()->GetZSize()) * SQUARE_SIZE;
	auto checkFacing = [this, map, terrainMgr, testSize, &predicate, &pos, searchRadius]() {
		AIFloat3 bp = terrainMgr->FindBuildSite(buildDef, pos, searchRadius, facing, predicate);
		if (!utils::is_valid(bp)) {
			return false;
		}

		// decides if a factory should face the opposite direction due to bad terrain
		AIFloat3 posOffset = bp;
		switch (facing) {
			default:
			case UNIT_FACING_SOUTH: {  // z++
				posOffset.z += testSize;
			} break;
			case UNIT_FACING_EAST: {  // x++
				posOffset.x += testSize;
			} break;
			case UNIT_FACING_NORTH: {  // z--
				posOffset.z -= testSize;
			} break;
			case UNIT_FACING_WEST: {  // x--
				posOffset.x -= testSize;
			} break;
		}
		if (map->IsPossibleToBuildAt(buildDef->GetDef(), posOffset, facing)) {
			SetBuildPos(bp);
			return true;
		}
		return false;
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

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, reprDefId);		\
	utils::binary_##func(stream, isPlop);

bool CBFactoryTask::Load(std::istream& is)
{
	CCircuitDef::Id reprDefId;

	IBuilderTask::Load(is);
	SERIALIZE(is, read)

	CCircuitAI* circuit = manager->GetCircuit();
	reprDef = circuit->GetCircuitDefSafe(reprDefId);

	circuit->GetFactoryManager()->AddFactory(buildDef);
	Activate();  // circuit->GetFactoryManager()->ApplySwitchFrame();
	return true;
}

void CBFactoryTask::Save(std::ostream& os) const
{
	CCircuitDef::Id reprDefId = (reprDef != nullptr) ? reprDef->GetId() : -1;

	IBuilderTask::Save(os);
	SERIALIZE(os, write)
}

} // namespace circuit
