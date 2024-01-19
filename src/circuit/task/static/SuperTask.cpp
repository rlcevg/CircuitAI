/*
 * SuperTask.cpp
 *
 *  Created on: Aug 12, 2016
 *      Author: rlcevg
 */

#include "task/static/SuperTask.h"
#include "task/fighter/SquadTask.h"
#include "map/InfluenceMap.h"
#include "module/MilitaryManager.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"
#include "Lua.h"

namespace circuit {

using namespace springai;

#define TARGET_DELAY	(FRAMES_PER_SEC * 10)

CSuperTask::CSuperTask(IUnitModule* mgr)
		: IFighterTask(mgr, IFighterTask::FightType::SUPER, 1.f)
		, targetFrame(0)
		, targetPos(-RgtVector)
{
}

CSuperTask::~CSuperTask()
{
}

bool CSuperTask::CanAssignTo(CCircuitUnit* unit) const
{
	return false;
}

void CSuperTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CSuperTask::Start(CCircuitUnit* unit)
{
	const int frame = manager->GetCircuit()->GetLastFrame();
	targetFrame = frame - TARGET_DELAY;
	position = unit->GetPos(frame);
}

void CSuperTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CCircuitUnit* unit = *units.begin();

	if (unit->Blocker() != nullptr) {
		return;  // Do not interrupt current action
	}

	CCircuitDef* cdef = unit->GetCircuitDef();
	if (cdef->IsHoldFire()) {
		if (targetFrame + (cdef->GetReloadTime() + TARGET_DELAY) > frame) {
			if ((State::ENGAGE == state) && (targetFrame + TARGET_DELAY <= frame)) {
				TRY_UNIT(circuit, unit,
					unit->CmdStop();
				)
				state = State::ROAM;
			}
			return;
		}
	} else if (targetFrame + TARGET_DELAY > frame) {
		return;
	}

	CInfluenceMap* inflMap = circuit->GetInflMap();
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	const float maxSqRange = SQUARE(cdef->GetMaxRange());
	const float sqAoe = SQUARE(cdef->GetAoe() * 1.25f);
	float cost = 0.f;
	int groupIdx = -1;
	const std::array<const std::set<IFighterTask*>*, 3> avoidTasks = {  // NOTE: ISquadTask only
		&militaryMgr->GetTasks(IFighterTask::FightType::ATTACK),
		&militaryMgr->GetTasks(IFighterTask::FightType::AH),
		&militaryMgr->GetTasks(IFighterTask::FightType::AA),
	};
	auto isAllySafe = [&avoidTasks, frame, sqAoe, inflMap](const AIFloat3& pos) {
		for (const std::set<IFighterTask*>* tasks : avoidTasks) {
			for (const IFighterTask* task : *tasks) {
				const AIFloat3& leaderPos = static_cast<const ISquadTask*>(task)->GetLeaderPos(frame);
				if (leaderPos.SqDistance2D(pos) < sqAoe) {
					return false;
				}
			}
		}
		return inflMap->GetInfluenceAt(pos) < -INFL_EPS;
	};
	const std::vector<CEnemyManager::SEnemyGroup>& groups = circuit->GetEnemyManager()->GetEnemyGroups();
	if (cdef->IsHoldFire() || (State::ROAM == state)) {
		for (unsigned i = 0; i < groups.size(); ++i) {
			const CEnemyManager::SEnemyGroup& group = groups[i];
			if ((cost >= group.cost) || (position.SqDistance2D(group.pos) >= maxSqRange)) {
				continue;
			}
			if (isAllySafe(group.pos)) {
				cost = group.cost;
				groupIdx = i;
			}
		}
	} else {
		// TODO: Use WeaponDef::GetTurnRate() for turn-delay weight
		const AIFloat3& targetVec = (targetPos - position).Normalize2D();
		for (unsigned i = 0; i < groups.size(); ++i) {
			const CEnemyManager::SEnemyGroup& group = groups[i];
			if (position.SqDistance2D(group.pos) >= maxSqRange) {
				continue;
			}
			const AIFloat3& newVec = (group.pos - position).Normalize2D();
			const float angleMod = M_PI / (2.f * (std::acos(targetVec.dot2D(newVec)) + 1e-2f));
			if (cost >= group.cost * angleMod) {
				continue;
			}
			if (isAllySafe(group.pos)) {
				cost = group.cost;
				groupIdx = i;
			}
		}
	}
	const float maxCost = cdef->IsAttrStock() ? cdef->GetWeaponDef()->GetCostM() : cdef->GetCostM() * 0.01f;
	if ((groupIdx < 0) || (cost < maxCost)) {
		TRY_UNIT(circuit, unit,
			unit->CmdStop();
		)
		SetTarget(nullptr);
		targetFrame = frame;
		return;
	}

	const AIFloat3& grPos = groups[groupIdx].pos;
	CEnemyInfo* bestTarget = nullptr;
	if (cdef->IsAttrStock()) {
		float minSqDist = std::numeric_limits<float>::max();
		for (const ICoreUnit::Id eId : groups[groupIdx].units) {
			CEnemyInfo* enemy = circuit->GetEnemyInfo(eId);
			if (enemy == nullptr) {
				continue;
			}
			const float sqDist = grPos.SqDistance2D(enemy->GetPos());
			if ((minSqDist > sqDist) && (position.SqDistance2D(enemy->GetPos()) < maxSqRange)) {
				minSqDist = sqDist;
				bestTarget = enemy;
			}
		}
	} else {
		float maxCost = 0.f;
		for (const ICoreUnit::Id eId : groups[groupIdx].units) {
			CEnemyInfo* enemy = circuit->GetEnemyInfo(eId);
			if (enemy == nullptr) {
				continue;
			}
			if ((maxCost < enemy->GetCost()) && (position.SqDistance2D(enemy->GetPos()) < maxSqRange)) {
				maxCost = enemy->GetCost();
				bestTarget = enemy;
			}
		}
	}
	SetTarget(bestTarget);
	if (GetTarget() != nullptr) {
		targetPos = GetTarget()->GetPos();
		targetPos.y = circuit->GetMap()->GetElevationAt(targetPos.x, targetPos.z);

		std::string cmd = (!cdef->IsAttrStock() || (unit->GetUnit()->GetStockpile() > 0)) ? "ai_super_fire:" : "ai_super_intention:";
		cmd += utils::int_to_string(unit->GetId()) + "/" + utils::int_to_string(targetPos.x) + "/" + utils::int_to_string(targetPos.z);
		circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

		TRY_UNIT(circuit, unit,
			if (GetTarget()->IsInRadarOrLOS() && !circuit->IsCheating()) {
				unit->GetUnit()->Attack(GetTarget()->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			} else {
				unit->CmdAttackGround(targetPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			}
		)
		targetFrame = frame;
		state = State::ENGAGE;
	}
}

} // namespace circuit
