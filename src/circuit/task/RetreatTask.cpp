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
#include "terrain/ThreatMap.h"
#include "unit/action/DGunAction.h"
#include "unit/action/MoveAction.h"
#include "unit/action/FightAction.h"
#include "unit/action/JumpAction.h"
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
}

void CRetreatTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	if (unit->HasDGun()) {
		CDGunAction* act = new CDGunAction(unit, unit->GetDGunRange() * 0.8f);
		unit->PushBack(act);
	}

	CCircuitAI* circuit = manager->GetCircuit();
	CCircuitDef* cdef = unit->GetCircuitDef();
	TRY_UNIT(circuit, unit,
		unit->GetUnit()->SetFireState(cdef->IsAttrRetHold() ? CCircuitDef::FireType::HOLD : CCircuitDef::FireType::OPEN);
	)
	if (cdef->IsAttrBoost()) {
		int frame = circuit->GetLastFrame() + FRAMES_PER_SEC * 60;
		TRY_UNIT(circuit, unit,
			if (cdef->IsPlane()) {
				unit->GetUnit()->ExecuteCustomCommand(CMD_FIND_PAD, {}, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame);
			}
			unit->GetUnit()->ExecuteCustomCommand(CMD_ONECLICK_WEAPON, {}, UNIT_COMMAND_OPTION_ALT_KEY | UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame);
		)
		return;
	}

	int squareSize = circuit->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (cdef->IsAbleToJump() && !cdef->IsAttrNoJump()) {
		travelAction = new CJumpAction(unit, squareSize);
	} else if (cdef->IsAttrRetFight()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushBack(travelAction);

	// Mobile repair
	if (!cdef->IsPlane()) {
		CBuilderManager* builderManager = circuit->GetBuilderManager();
		builderManager->EnqueueRepair(IBuilderTask::Priority::HIGH, unit);
	}
}

void CRetreatTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->DoneTask(this);
	}

	TRY_UNIT(manager->GetCircuit(), unit,
		unit->GetUnit()->SetFireState(unit->GetCircuitDef()->GetFireState());
	)
}

void CRetreatTask::Start(CCircuitUnit* unit)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (!act->IsAny(IUnitAction::Mask::MOVE | IUnitAction::Mask::FIGHT | IUnitAction::Mask::JUMP)) {
		return;
	}
	ITravelAction* travelAction = static_cast<ITravelAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
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
		if (!utils::is_valid(endPos)) {
			endPos = circuit->GetSetupManager()->GetBasePos();
		}
		range = factoryManager->GetAssistDef()->GetBuildDistance() * 0.6f + pathfinder->GetSquareSize();
	}
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();

	const float minThreat = circuit->GetThreatMap()->GetUnitThreat(unit) * 0.125f;
	pathfinder->SetMapData(unit, circuit->GetThreatMap(), frame);
	pathfinder->MakePath(*pPath, startPos, endPos, range, minThreat);

	if (pPath->empty()) {
		pPath->push_back(endPos);
	}
	travelAction->SetPath(pPath);
}

void CRetreatTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	bool isExecute = (++updCount % 2 == 0);
	auto assignees = units;
	for (CCircuitUnit* unit : assignees) {
		const float healthPerc = unit->GetHealthPercent();
		bool isRepaired;
		if (unit->HasShield()) {
			isRepaired = (healthPerc > 0.98f) && unit->IsShieldCharged(circuit->GetSetupManager()->GetFullShield());
		} else {
			isRepaired = healthPerc > 0.98f;
		}

		if (isRepaired && !unit->IsDisarmed(frame)) {
			RemoveAssignee(unit);
		} else if (unit->IsForceExecute() || isExecute) {
			Start(unit);
		}
	}
}

void CRetreatTask::Finish()
{
	Cancel();
}

void CRetreatTask::Cancel()
{
	if (repairer != nullptr) {
		IUnitTask* repairerTask = repairer->GetTask();
		repairerTask->GetManager()->AbortTask(repairerTask);
	}
}

void CRetreatTask::OnUnitIdle(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsPlane()) {
		// NOTE: unit considered idle after boost and find_pad
		if (State::REGROUP == state) {
			state = State::ROAM;
			return;
		}
		IUnitAction* act = static_cast<IUnitAction*>(unit->End());
		if (act->IsAny(IUnitAction::Mask::MOVE | IUnitAction::Mask::FIGHT | IUnitAction::Mask::JUMP)) {
			static_cast<ITravelAction*>(act)->SetFinished(true);
		}

		TRY_UNIT(circuit, unit,
			unit->GetUnit()->ExecuteCustomCommand(CMD_FIND_PAD, {}, UNIT_COMMAND_OPTION_INTERNAL_ORDER, frame + FRAMES_PER_SEC * 60);
		)
		state = State::REGROUP;
		return;
	}

	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	AIFloat3 haven = (repairer != nullptr) ? repairer->GetPos(frame) : factoryManager->GetClosestHaven(unit);
	if (!utils::is_valid(haven)) {
		haven = circuit->GetSetupManager()->GetBasePos();
	}

	const float maxDist = factoryManager->GetAssistDef()->GetBuildDistance();
	const AIFloat3& unitPos = unit->GetPos(frame);
	if (unitPos.SqDistance2D(haven) > maxDist * maxDist) {
		// TODO: push MoveAction into unit? to avoid enemy fire
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->MoveTo(haven, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 1);
		)
		// TODO: Add fail counter?
	} else {
		// TODO: push WaitAction into unit
		AIFloat3 pos = unitPos;
		const float size = SQUARE_SIZE * 16;
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		float centerX = terrainManager->GetTerrainWidth() / 2;
		float centerZ = terrainManager->GetTerrainHeight() / 2;
		pos.x += (pos.x > centerX) ? size : -size;
		pos.z += (pos.z > centerZ) ? size : -size;
		AIFloat3 oldPos = pos;
		CTerrainManager::CorrectPosition(pos);
		if (oldPos.SqDistance2D(pos) > SQUARE_SIZE * SQUARE_SIZE) {
			pos = unitPos;
			pos.x += (pos.x > centerX) ? -size : size;
			pos.z += (pos.z > centerZ) ? -size : size;
		}
		CTerrainManager::TerrainPredicate predicate = [unitPos](const AIFloat3& p) {
			return unitPos.SqDistance2D(p) > SQUARE(SQUARE_SIZE * 8);
		};
		pos = terrainManager->FindBuildSite(cdef, pos, maxDist, UNIT_COMMAND_BUILD_NO_FACING, predicate);
		TRY_UNIT(circuit, unit,
//			unit->GetUnit()->ExecuteCustomCommand(CMD_PRIORITY, {0.0f});
			unit->GetUnit()->PatrolTo(pos);
		)

		IUnitAction* act = static_cast<IUnitAction*>(unit->End());
		if (act->IsAny(IUnitAction::Mask::MOVE | IUnitAction::Mask::FIGHT | IUnitAction::Mask::JUMP)) {
			static_cast<ITravelAction*>(act)->SetFinished(true);
		}
		state = State::REGROUP;
	}
}

void CRetreatTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	if (State::REGROUP != state) {
		return;
	}
	state = State::ROAM;

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsAbleToJump() && !cdef->IsAttrNoJump()) {
		travelAction = new CJumpAction(unit, squareSize);
	} else if (cdef->IsAttrRetFight()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushBack(travelAction);

	Start(unit);
}

void CRetreatTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
}

void CRetreatTask::CheckRepairer(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	AIFloat3 startPos = (*units.begin())->GetPos(frame);
	AIFloat3 endPos;
	float range;

	bool isRepairer = (repairer != nullptr);
	if (isRepairer) {
		endPos = repairer->GetPos(frame);
		range = pathfinder->GetSquareSize();
	} else {
		CFactoryManager* factoryManager = circuit->GetFactoryManager();
		endPos = factoryManager->GetClosestHaven(unit);
		if (!utils::is_valid(endPos)) {
			endPos = circuit->GetSetupManager()->GetBasePos();
		}
		range = factoryManager->GetAssistDef()->GetBuildDistance() * 0.6f + pathfinder->GetSquareSize();
	}

//	CTerrainManager::CorrectPosition(startPos);
	pathfinder->SetMapData(unit, circuit->GetThreatMap(), frame);
	float prevCost = pathfinder->PathCost(startPos, endPos, range);
	if (isRepairer && repairer->GetCircuitDef()->IsMobile()) {
		prevCost /= 2;
	}

	endPos = unit->GetPos(frame);
	float nextCost = pathfinder->PathCost(startPos, endPos, range);
	if (unit->GetCircuitDef()->IsMobile()) {
		nextCost /= 2;
	}

	if (prevCost > nextCost) {
		SetRepairer(unit);
	}
}

} // namespace circuit
