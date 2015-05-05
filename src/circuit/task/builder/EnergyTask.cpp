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
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBEnergyTask::CBEnergyTask(ITaskManager* mgr, Priority priority,
						   CCircuitDef* buildDef, const AIFloat3& position,
						   float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::ENERGY, cost, timeout)
{
}

CBEnergyTask::~CBEnergyTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBEnergyTask::Finish()
{
	// FIXME: Replace const 1000.0f with build time?
	if (cost <= 1000.0f) {
		return;
	}

	// TODO: Implement BuildWait action - semaphore for group of tasks
	CCircuitAI* circuit = manager->GetCircuit();
	AIFloat3 buildPos = this->buildPos;
	UnitDef* unitDef = buildDef->GetUnitDef();
	float offsetX = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) * SQUARE_SIZE + 10 * SQUARE_SIZE;
	float offsetZ = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) * SQUARE_SIZE + 10 * SQUARE_SIZE;
	int facing = this->facing;
	int targetId = target->GetId();
	// FIXME: Using builder's def because MaxSlope is not provided by engine's interface for buildings!
	//        and CTerrainManager::CanBuildAt returns false in many cases
	CCircuitDef* bdef = (*units.begin())->GetCircuitDef();
//	auto defenceGroup = [circuit, buildPos, offsetX, offsetZ, facing, targetId, bdef]() {
		if (circuit->GetTeamUnit(targetId) == nullptr) {
			return;
		}
		CBuilderManager* builderManager = circuit->GetBuilderManager();
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		CCircuitDef* cdef, *fdef;
		AIFloat3 pos;

		cdef = circuit->GetCircuitDef("corllt");
		pos = buildPos + AIFloat3(-offsetX, 0, -offsetZ);
		pos = terrainManager->GetBuildPosition(bdef, pos);
		builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);
		pos = buildPos + AIFloat3(+offsetX, 0, +offsetZ);
		pos = terrainManager->GetBuildPosition(bdef, pos);
		builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::DEFENCE);

		cdef = circuit->GetCircuitDef("corgrav");
		fdef = circuit->GetCircuitDef("armartic");
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
				pos.z -= offsetZ;
				break;
			case UNIT_FACING_EAST:
				pos.x -= offsetX;
				break;
			case UNIT_FACING_NORTH:
				pos.z += offsetZ;
				break;
			case UNIT_FACING_WEST:
				pos.x += offsetX;
				break;
		}
		pos = terrainManager->GetBuildPosition(bdef, pos);
		builderManager->EnqueueTask(IBuilderTask::Priority::NORMAL, cdef, pos, IBuilderTask::BuildType::NANO);
//	};
//	manager->GetCircuit()->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>(defenceGroup), FRAMES_PER_SEC * 60);

	IBuilderTask::Finish();
}

} // namespace circuit
