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
	float ourRange = circuit->GetEnergyGrid()->GetPylonRange(buildDef->GetId());
	float pylonRange = economyManager->GetPylonRange();
	circuit->UpdateFriendlyUnits();
	float radius = pylonRange + ourRange;
	auto units = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(buildPos, radius));
	for (auto u : units) {
		CCircuitUnit* p = circuit->GetFriendlyUnit(u);
		if (p == nullptr) {
			continue;
		}
		// FIXME: Is SqDistance2D necessary? Or must subtract model radius of pylon from "radius" variable
		//        @see rts/Sim/Misc/QaudField.cpp
		//        ...CQuadField::GetUnitsExact(const float3& pos, float radius, bool spherical)
		//        const float totRad = radius + u->radius; -- suspicious
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
			const AIFloat3& clPos = metalManager->GetClusters()[index].geoCentr;
			AIFloat3 dir = clPos - pos;
			float dist = ourRange /*+ pylonRange*/ + pylonRange * 1.8f;
			if (dir.SqLength2D() < dist * dist) {
				pos = (pos /*+ dir.Normalize2D() * (ourRange - pylonRange)*/ + clPos) * 0.5f;
			} else {
				pos += dir.Normalize2D() * (ourRange + pylonRange) * 0.9f;
			}
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
	CCircuitDef* bdef = units.empty() ? circuit->GetCircuitDef("armrectr") : (*this->units.begin())->GetCircuitDef();

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CCircuitDef* cdef, *fdef;
	AIFloat3 pos;
	IBuilderTask* parent0, *parent1;

	cdef = circuit->GetCircuitDef("corllt");
	fdef = circuit->GetCircuitDef("armartic");
	pos = buildPos + AIFloat3(-offsetX, 0, -offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent0 = builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent0 = parent0->GetNextTask();
	pos = buildPos + AIFloat3(+offsetX, 0, +offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent1 = builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
	parent1->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent1 = parent1->GetNextTask();

	cdef = circuit->GetCircuitDef("corgrav");
	fdef = circuit->GetCircuitDef("corrl");
	pos = buildPos + AIFloat3(-offsetX * 0.7, 0, +offsetZ * 0.7);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent0 = parent0->GetNextTask();
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent0 = parent0->GetNextTask();
	pos = buildPos + AIFloat3(+offsetX * 0.7, 0, -offsetZ * 0.7);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent1->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent1 = parent1->GetNextTask();
	parent1->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, fdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent1 = parent1->GetNextTask();

	cdef = circuit->GetCircuitDef("missiletower");
	pos = buildPos + AIFloat3(-offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent0 = parent0->GetNextTask();
	pos = buildPos + AIFloat3(+offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent0 = parent0->GetNextTask();
	pos = buildPos + AIFloat3(0, 0, -offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent1->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent1 = parent1->GetNextTask();
	pos = buildPos + AIFloat3(0, 0, +offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent1->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent1 = parent1->GetNextTask();

	cdef = circuit->GetCircuitDef("corjamt");
	pos = buildPos + AIFloat3(-offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent0 = parent0->GetNextTask();
	pos = buildPos + AIFloat3(+offsetX, 0, 0);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent0 = parent0->GetNextTask();
	pos = buildPos + AIFloat3(0, 0, -offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent1->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent1 = parent1->GetNextTask();
	pos = buildPos + AIFloat3(0, 0, +offsetZ);
	pos = terrainManager->GetBuildPosition(bdef, pos);
	parent1->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE, true, false));
	parent1 = parent1->GetNextTask();

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
	parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::NANO, false, false));
	parent0 = parent0->GetNextTask();

	if (rand() < RAND_MAX / 2) {
		cdef = circuit->GetCircuitDef("armamd");
		parent0->SetNextTask(builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::BIG_GUN, true, false));
	}
}

} // namespace circuit
