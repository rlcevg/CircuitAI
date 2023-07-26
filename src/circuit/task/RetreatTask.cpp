/*
 * RetreatTask.cpp
 *
 *  Created on: Jan 18, 2015
 *      Author: rlcevg
 */

#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/FactoryManager.h"
#include "setup/SetupManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathSingle.h"
#include "terrain/path/QueryCostMap.h"
#include "terrain/TerrainManager.h"
#include "unit/action/DGunAction.h"
#include "unit/action/MoveAction.h"
#include "unit/action/FightAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

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

void CRetreatTask::ClearRelease()
{
	costQuery = nullptr;
	IUnitTask::ClearRelease();
}

void CRetreatTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);

	if (unit->HasDGun()) {
		unit->PushDGunAct(new CDGunAction(unit, unit->GetDGunRange() * 0.8f));
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
				unit->CmdFindPad(frame);
			}
			unit->CmdManualFire(UNIT_COMMAND_OPTION_ALT_KEY, frame);
		)
		return;
	}

	int squareSize = circuit->GetPathfinder()->GetSquareSize();
	ITravelAction* travelAction;
	if (cdef->IsAttrRetFight()) {
		travelAction = new CFightAction(unit, squareSize);
	} else {
		travelAction = new CMoveAction(unit, squareSize);
	}
	unit->PushTravelAct(travelAction);
	unit->SetAllowedToJump(cdef->IsAbleToJump() && !cdef->IsAttrNoJump());

	// Mobile repair
	if (!cdef->IsPlane()) {
		circuit->GetBuilderManager()->EnqueueRepair(IBuilderTask::Priority::HIGH, unit);
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
	if ((unit->GetTravelAct() == nullptr) || unit->GetTravelAct()->IsFinished()) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	const AIFloat3& startPos = unit->GetPos(frame);
	AIFloat3 endPos;
	float range;

	if (repairer != nullptr) {
		endPos = repairer->GetPos(frame);
		range = pathfinder->GetSquareSize();
	} else {
		CFactoryManager* factoryMgr = circuit->GetFactoryManager();
		endPos = factoryMgr->GetClosestHaven(unit);
		if (!utils::is_valid(endPos)) {
			endPos = circuit->GetSetupManager()->GetBasePos();
		}
		range = factoryMgr->GetAssistDef()->GetBuildDistance() * 0.6f + pathfinder->GetSquareSize();
	}

	if (unit->GetTravelAct()->GetPath() == nullptr) {
		std::shared_ptr<PathInfo> pPath = std::make_shared<PathInfo>();
		pPath->posPath.push_back(endPos);
		unit->GetTravelAct()->SetPath(pPath);
	}

	if (!IsQueryReady(unit)) {
		return;
	}

//	const float minThreat = circuit->GetThreatMap()->GetUnitThreat(unit) * 0.125f;
	std::shared_ptr<IPathQuery> query = pathfinder->CreatePathSingleQuery(
			unit, circuit->GetThreatMap(), frame,
			startPos, endPos, range/*, nullptr, minThreat*/);
	pathQueries[unit] = query;
	query->HoldTask(this);

	pathfinder->RunQuery(query, [this](const IPathQuery* query) {
		if (this->IsQueryAlive(query)) {
			this->ApplyPath(static_cast<const CQueryPathSingle*>(query));
		}
	});
}

void CRetreatTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	bool isExecute = (++updCount % 2 == 0);
	auto assignees = units;
	for (CCircuitUnit* unit : assignees) {
		const float healthPerc = unit->GetHealthPercent();
		bool isRepaired = unit->HasShield()
				? (healthPerc > 0.98f) && unit->IsShieldCharged(circuit->GetSetupManager()->GetFullShield())
				: healthPerc > 0.98f;

		if (isRepaired && !unit->IsDisarmed(frame)) {
			RemoveAssignee(unit);
		} else if (unit->IsForceUpdate(frame) || isExecute) {
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
		if (unit->GetTravelAct() != nullptr) {
			unit->GetTravelAct()->StateFinish();
		}

		TRY_UNIT(circuit, unit,
			unit->CmdFindPad(frame + FRAMES_PER_SEC * 60);
		)
		state = State::REGROUP;
		return;
	}

	CFactoryManager* factoryMgr = circuit->GetFactoryManager();
	AIFloat3 haven = (repairer != nullptr) ? repairer->GetPos(frame) : factoryMgr->GetClosestHaven(unit);
	if (!utils::is_valid(haven)) {
		haven = circuit->GetSetupManager()->GetBasePos();
	}

	const float maxDist = factoryMgr->GetAssistDef()->GetBuildDistance();
	const AIFloat3& unitPos = unit->GetPos(frame);
	if (unitPos.SqDistance2D(haven) > maxDist * maxDist) {
		// TODO: push MoveAction into unit? to avoid enemy fire
		TRY_UNIT(circuit, unit,
			unit->CmdMoveTo(haven, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 1);
		)
		// TODO: Add fail counter?
	} else {
		// TODO: push WaitAction into unit
		AIFloat3 pos = unitPos;
		const float size = SQUARE_SIZE * 16;
		CTerrainManager* terrainMgr = circuit->GetTerrainManager();
		float centerX = terrainMgr->GetTerrainWidth() / 2;
		float centerZ = terrainMgr->GetTerrainHeight() / 2;
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
		pos = terrainMgr->FindBuildSite(cdef, pos, maxDist, UNIT_COMMAND_BUILD_NO_FACING, predicate);
		TRY_UNIT(circuit, unit,
//			unit->CmdPriority(0);
			unit->GetUnit()->PatrolTo(pos);
		)

		if (unit->GetTravelAct() != nullptr) {
			unit->GetTravelAct()->StateFinish();
		}
		state = State::REGROUP;
	}
}

void CRetreatTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	if (State::REGROUP != state) {
		return;
	}
	state = State::ROAM;

	if (unit->GetTravelAct() == nullptr) {
		// NOTE: IsAttrBoost units don't get travel action on AssignTo
		int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
		CCircuitDef* cdef = unit->GetCircuitDef();
		ITravelAction* travelAction;
		if (cdef->IsAttrRetFight()) {
			travelAction = new CFightAction(unit, squareSize);
		} else {
			travelAction = new CMoveAction(unit, squareSize);
		}
		unit->PushTravelAct(travelAction);
		unit->SetAllowedToJump(cdef->IsAbleToJump() && !cdef->IsAttrNoJump());
	}
	unit->GetTravelAct()->StateActivate();

	Start(unit);
}

void CRetreatTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	RemoveAssignee(unit);
}

void CRetreatTask::CheckRepairer(CCircuitUnit* newRep)
{
	CCircuitUnit* unit = *units.begin();

	if ((costQuery != nullptr) && (costQuery->GetState() != IPathQuery::State::READY)) {  // not ready
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& startPos = unit->GetPos(frame);

	CPathFinder* pathfinder = circuit->GetPathfinder();
	costQuery = pathfinder->CreateCostMapQuery(
			unit, circuit->GetThreatMap(), frame, startPos);
	costQuery->HoldTask(this);

	CCircuitUnit::Id newRepId = newRep->GetId();
	pathfinder->RunQuery(costQuery, [this, newRepId](const IPathQuery* query) {
		CCircuitUnit* newRep = this->ValidateNewRepairer(query, newRepId);
		if (newRep != nullptr) {
			this->ApplyCostMap(static_cast<const CQueryCostMap*>(query), newRep);
		}
	});
}

void CRetreatTask::ApplyPath(const CQueryPathSingle* query)
{
	const std::shared_ptr<PathInfo>& pPath = query->GetPathInfo();
	CCircuitUnit* unit = query->GetUnit();

	if (pPath->posPath.empty()) {
		pPath->posPath.push_back(query->GetEndPos());
	}
	unit->GetTravelAct()->SetPath(pPath);
}

CCircuitUnit* CRetreatTask::ValidateNewRepairer(const IPathQuery* query, int newRepId) const
{
	if (isDead || (costQuery == nullptr) || (costQuery->GetId() != query->GetId())) {
		return nullptr;
	}
	CCircuitUnit* newRep = manager->GetCircuit()->GetTeamUnit(newRepId);
	if (newRep == nullptr) {
		return nullptr;
	}
	if (newRep->GetTask()->GetType() != IUnitTask::Type::BUILDER) {
		return nullptr;
	}
	IBuilderTask* taskB = static_cast<IBuilderTask*>(newRep->GetTask());
	if ((taskB->GetBuildType() != IBuilderTask::BuildType::REPAIR) || (taskB->GetTarget() != query->GetUnit())) {
		return nullptr;
	}
	return newRep;
}

void CRetreatTask::ApplyCostMap(const CQueryCostMap* query, CCircuitUnit* newRep)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	CCircuitUnit* unit = query->GetUnit();
	AIFloat3 endPos;
	float range;

	bool isRepairer = (repairer != nullptr);
	if (isRepairer) {
		endPos = repairer->GetPos(frame);
		range = pathfinder->GetSquareSize();
	} else {
		CFactoryManager* factoryMgr = circuit->GetFactoryManager();
		endPos = factoryMgr->GetClosestHaven(unit);
		if (!utils::is_valid(endPos)) {
			endPos = circuit->GetSetupManager()->GetBasePos();
		}
		range = factoryMgr->GetAssistDef()->GetBuildDistance() * 0.6f + pathfinder->GetSquareSize();
	}

	float prevCost = query->GetCostAt(endPos, range);
	if (isRepairer && repairer->GetCircuitDef()->IsMobile()) {
		prevCost /= 2;
	}

	endPos = unit->GetPos(frame);
	float nextCost = query->GetCostAt(endPos, range);
	if (unit->GetCircuitDef()->IsMobile()) {
		nextCost /= 2;
	}

	if (prevCost > nextCost) {
		SetRepairer(newRep);
	}
}

} // namespace circuit
