/*
 * RetreatTask.cpp
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "setup/SetupManager.h"
#include "terrain/PathFinder.h"
#include "terrain/TerrainManager.h"
#include "unit/action/DGunAction.h"
#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CRetreatTask::CRetreatTask(ITaskManager* mgr, int timeout)
		: IUnitTask(mgr, Priority::NORMAL, Type::RETREAT, timeout)
		, repairer(nullptr)
{
}

CRetreatTask::~CRetreatTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CRetreatTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	if (unit->HasDGun()) {
		CDGunAction* act = new CDGunAction(unit, unit->GetCircuitDef()->GetDGunRange() * 0.8f);
		unit->PushBack(act);
	}

	if (!unit->GetCircuitDef()->IsPlane()) {
		unit->PushBack(new CMoveAction(unit));

		// Mobile repair
		CBuilderManager* builderManager = manager->GetCircuit()->GetBuilderManager();
		builderManager->EnqueueRepair(IBuilderTask::Priority::NORMAL, unit);
	}
}

void CRetreatTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	if (units.empty()) {
		manager->DoneTask(this);
	}
}

void CRetreatTask::Execute(CCircuitUnit* unit)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (act->GetType() != IUnitAction::Type::MOVE) {
		return;
	}
	CMoveAction* moveAction = static_cast<CMoveAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	AIFloat3 startPos = unit->GetPos(frame);
	AIFloat3 endPos;
	float range;

	if (repairer != nullptr) {
		endPos = repairer->GetPos(frame);
		range = pathfinder->GetSquareSize();
	} else {
		CFactoryManager* factoryManager = circuit->GetFactoryManager();
		endPos = factoryManager->GetClosestHaven(unit);
		if (endPos == -RgtVector) {
			endPos = circuit->GetSetupManager()->GetBasePos();
		}
		range = factoryManager->GetAssistDef()->GetBuildDistance() * 0.6f + pathfinder->GetSquareSize();
	}
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();

	pathfinder->SetMapData(unit, circuit->GetThreatMap(), frame);
	pathfinder->MakePath(*pPath, startPos, endPos, range);

	if (pPath->empty()) {
		pPath->push_back(endPos);
	}
	moveAction->SetPath(pPath);
	unit->Update(circuit);
}

void CRetreatTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	bool isExecute = (++updCount % 4 == 0);
	auto assignees = units;
	for (CCircuitUnit* unit : assignees) {
		Unit* u = unit->GetUnit();
		const float healthPerc = u->GetHealth() / u->GetMaxHealth();
		bool isRepaired;
		// FIXME: Wait until 101.0 engine
//		if (ass->GetShield() != nullptr) {
//			isRepaired = (healthPerc > 0.98f) && unit->IsShieldCharged(0.9f));
//		} else {
			isRepaired = healthPerc > 0.98f;
//		}

		if (isRepaired && !unit->IsDisarmed(frame)) {
			RemoveAssignee(unit);
		} else if (unit->IsForceExecute() || isExecute) {
			Execute(unit);
		} else {
			unit->Update(circuit);
		}
	}
}

void CRetreatTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	AIFloat3 haven = (repairer != nullptr) ? repairer->GetPos(frame) : factoryManager->GetClosestHaven(unit);
	if (haven == -RgtVector) {
		haven = circuit->GetSetupManager()->GetBasePos();
	}

	if (unit->GetCircuitDef()->IsPlane()) {
		// force rearm/repair | CMD_FIND_PAD
		unit->GetUnit()->Fight(haven, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		return;
	}

	const float maxDist = factoryManager->GetAssistDef()->GetBuildDistance();
	const AIFloat3& unitPos = unit->GetPos(frame);
	if (unitPos.SqDistance2D(haven) > maxDist * maxDist) {
		// TODO: push MoveAction into unit? to avoid enemy fire
		unit->GetUnit()->MoveTo(haven, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 1);
		// TODO: Add fail counter?
	} else {
		// TODO: push WaitAction into unit
//		unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {0.0f});

		AIFloat3 pos = unitPos;
		const float size = SQUARE_SIZE * 16;
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		float centerX = terrainManager->GetTerrainWidth() / 2;
		float centerZ = terrainManager->GetTerrainHeight() / 2;
		pos.x += (pos.x > centerX) ? size : -size;
		pos.z += (pos.z > centerZ) ? size : -size;
		AIFloat3 oldPos = pos;
		terrainManager->CorrectPosition(pos);
		if (oldPos.SqDistance2D(pos) > SQUARE_SIZE * SQUARE_SIZE) {
			pos = unitPos;
			pos.x += (pos.x > centerX) ? -size : size;
			pos.z += (pos.z > centerZ) ? -size : size;
		}
		CTerrainManager::TerrainPredicate predicate = [unitPos](const AIFloat3& p) {
			return unitPos.SqDistance2D(p) > SQUARE(SQUARE_SIZE * 8);
		};
		pos = terrainManager->FindBuildSite(unit->GetCircuitDef(), pos, maxDist, UNIT_COMMAND_BUILD_NO_FACING, predicate);
		unit->GetUnit()->PatrolTo(pos);

		IUnitAction* act = static_cast<IUnitAction*>(unit->End());
		if (act->GetType() == IUnitAction::Type::MOVE) {
			static_cast<CMoveAction*>(act)->SetFinished(true);
		}
	}
}

void CRetreatTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	//TODO: Rebuild retreat path?
}

void CRetreatTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
}

void CRetreatTask::CheckRepairer(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	AIFloat3 startPos = (*units.begin())->GetPos(frame);
	AIFloat3 endPos;
	float range;

	bool isRepairer = repairer != nullptr;
	if (isRepairer) {
		endPos = repairer->GetPos(frame);
		range = pathfinder->GetSquareSize();
	} else {
		CFactoryManager* factoryManager = circuit->GetFactoryManager();
		endPos = factoryManager->GetClosestHaven(unit);
		if (endPos == -RgtVector) {
			endPos = circuit->GetSetupManager()->GetBasePos();
		}
		range = factoryManager->GetAssistDef()->GetBuildDistance() * 0.6f + pathfinder->GetSquareSize();
	}

	circuit->GetTerrainManager()->CorrectPosition(startPos);
	pathfinder->SetMapData(unit, circuit->GetThreatMap(), frame);
	float prevCost = pathfinder->PathCost(startPos, endPos, range);
	if (isRepairer && repairer->GetCircuitDef()->IsMobile()) {
		prevCost /= 4;
	}

	endPos = unit->GetPos(frame);
	float nextCost = pathfinder->PathCost(startPos, endPos, range);
	if (unit->GetCircuitDef()->IsMobile()) {
		nextCost /= 4;
	}

	if (prevCost > nextCost) {
		SetRepairer(unit);
	}
}

} // namespace circuit
