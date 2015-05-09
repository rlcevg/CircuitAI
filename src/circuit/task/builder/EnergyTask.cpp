/*
 * EnergyTask.cpp
 *
 *  Created on: Jan 30, 2015
 *      Author: rlcevg
 */

#include "task/builder/EnergyTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "module/EconomyManager.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "OOAICallback.h"

namespace circuit {

using namespace springai;

CBEnergyTask::CBEnergyTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   float cost, bool isShake, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::ENERGY, cost, isShake, timeout)
{
}

CBEnergyTask::~CBEnergyTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBEnergyTask::Finish()
{
	IBuilderTask::Finish();

	// FIXME: Replace const 1000.0f with build time?
	if (cost < 1000.0f) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	CBuilderManager* builderManager = circuit->GetBuilderManager();

	bool foundPylon = false;
	CEconomyManager* economyManager = circuit->GetEconomyManager();
	CCircuitDef* pylonDef = economyManager->GetPylonDef();
	float pylonRange = circuit->GetEnergyGrid()->GetPylonRange(buildDef->GetId());
	circuit->UpdateFriendlyUnits();
	float radius = economyManager->GetPylonRange() + pylonRange;
	auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(buildPos, radius));
	for (auto u : units) {
		CCircuitUnit* p = circuit->GetFriendlyUnit(u);
		if (p == nullptr) {
			continue;
		}
		if ((*p->GetCircuitDef() == *pylonDef) && (buildPos.SqDistance2D(u->GetPos()) < radius * radius)) {
			foundPylon = true;
			break;
		}
	}
	utils::free_clear(units);
	if (!foundPylon) {
		AIFloat3 pos = buildPos;
		CMetalManager* metalManager = circuit->GetMetalManager();
		int index = metalManager->FindNearestCluster(pos);
		if (index >= 0) {
			AIFloat3 dir = metalManager->GetClusters()[index].geoCentr - pos;
			pos += dir.Normalize2D() * pylonRange;
		}
		builderManager->EnqueuePylon(IBuilderTask::Priority::HIGH, pylonDef, pos, nullptr, 1.0f);
	}

	if (cost < 1001.0f) {
		return;
	}

	// TODO: Implement BuildWait action - semaphore for group of tasks / task's queue
	UnitDef* unitDef = buildDef->GetUnitDef();
	float offsetX = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) * SQUARE_SIZE + 10 * SQUARE_SIZE;
	float offsetZ = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) * SQUARE_SIZE + 10 * SQUARE_SIZE;
	// FIXME: Using builder's def because MaxSlope is not provided by engine's interface for buildings!
	//        and CTerrainManager::CanBuildAt returns false in many cases
	CCircuitDef* bdef = (*this->units.begin())->GetCircuitDef();

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* cdef, *fdef;
	AIFloat3 pos;

	cdef = circuit->GetCircuitDef("corllt");
	fdef = circuit->GetCircuitDef("armartic");
	pos = buildPos + AIFloat3(-offsetX, 0, -offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(+offsetX, 0, +offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE);

	cdef = circuit->GetCircuitDef("corgrav");
	fdef = circuit->GetCircuitDef("corrl");
	pos = buildPos + AIFloat3(-offsetX * 0.7, 0, +offsetZ * 0.7);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(+offsetX * 0.7, 0, -offsetZ * 0.7);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE);

	cdef = circuit->GetCircuitDef("missiletower");
	pos = buildPos + AIFloat3(-offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(+offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(0, 0, -offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(0, 0, +offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);

	cdef = circuit->GetCircuitDef("corjamt");
	pos = buildPos + AIFloat3(-offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(+offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(0, 0, -offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	pos = buildPos + AIFloat3(0, 0, +offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);

	cdef = circuit->GetFactoryManager()->GetAssistDef();
	pos = buildPos;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
			pos.z -= offsetZ / 2;
			break;
		case UNIT_FACING_EAST:
			pos.x -= offsetX / 2;
			break;
		case UNIT_FACING_NORTH:
			pos.z += offsetZ / 2;
			break;
		case UNIT_FACING_WEST:
			pos.x += offsetX / 2;
			break;
	}
	pos = terrainManager->GetBuildPosition(bdef, pos);
	builderManager->EnqueueTask(IBuilderTask::Priority::HIGH, cdef, pos, IBuilderTask::BuildType::NANO, false);
}

} // namespace circuit
