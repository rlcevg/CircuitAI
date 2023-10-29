/*
 * MilitaryTask.cpp
 *
 *  Created on: Apr 10, 2020
 *      Author: rlcevg
 */

#include "task/builder/CombatTask.h"
#include "task/RetreatTask.h"
#include "map/InfluenceMap.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "unit/action/DGunAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;
using namespace terrain;

CCombatTask::CCombatTask(ITaskManager* mgr, float powerMod)
		: IFighterTask(mgr, FightType::DEFEND, powerMod)
{
}

CCombatTask::~CCombatTask()
{
}

void CCombatTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CCombatTask::Start(CCircuitUnit* unit)
{
	Execute(unit);
}

void CCombatTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	if ((++updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC)) {
		lastTouched = frame;
		decltype(units) tmpUnits = units;
		for (CCircuitUnit* unit : tmpUnits) {
			Execute(unit);
		}
	}
}

void CCombatTask::OnUnitIdle(CCircuitUnit* unit)
{
	auto it = cowards.find(unit);
	if (it != cowards.end()) {
		cowards.erase(it);
		CRetreatTask* task = manager->GetCircuit()->GetBuilderManager()->EnqueueRetreat();
		manager->AssignTask(unit, task);
	} else {
		unit->SetTaskFrame(manager->GetCircuit()->GetLastFrame());
	}
}

void CCombatTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CCircuitDef* cdef = unit->GetCircuitDef();

	// FIXME: comm kamikaze
	if (cdef->IsRoleComm() && (GetTarget() != nullptr) && GetTarget()->IsInLOS() && GetTarget()->GetCircuitDef()->IsRoleComm()) {
		return;
	}

	const float healthPerc = unit->GetHealthPercent();
	if (unit->HasShield()) {
		const float minShield = circuit->GetSetupManager()->GetEmptyShield();
		if ((healthPerc > cdef->GetRetreat()) && unit->IsShieldCharged(minShield)) {
			if (cdef->IsRoleHeavy() && (healthPerc < 0.9f)) {
				circuit->GetBuilderManager()->Enqueue(TaskB::Repair(IBuilderTask::Priority::NOW, unit));
			}
			return;
		}
	} else if ((healthPerc > cdef->GetRetreat()) && !unit->IsDisarmed(frame)) {
		if (cdef->IsRoleHeavy() && (healthPerc < 0.9f)) {
			circuit->GetBuilderManager()->Enqueue(TaskB::Repair(IBuilderTask::Priority::NOW, unit));
		}
		return;
	} else if (healthPerc < 0.2f) {  // stuck units workaround: they don't shoot and don't see distant threat
		CRetreatTask* task = circuit->GetBuilderManager()->EnqueueRetreat();
		manager->AssignTask(unit, task);
		return;
	}

	CThreatMap* threatMap = circuit->GetThreatMap();
	const float range = cdef->GetMaxRange();
	if ((GetTarget() == nullptr) || !GetTarget()->IsInLOS()) {
		CRetreatTask* task = circuit->GetBuilderManager()->EnqueueRetreat();
		manager->AssignTask(unit, task);
		return;
	}
	const AIFloat3& pos = unit->GetPos(frame);
	if ((GetTarget()->GetPos().SqDistance2D(pos) > SQUARE(range))
		|| (threatMap->GetThreatAt(unit, pos) * 2 > threatMap->GetUnitPower(unit)))
	{
		CRetreatTask* task = circuit->GetBuilderManager()->EnqueueRetreat();
		manager->AssignTask(unit, task);
		return;
	}
	cowards.insert(unit);
}

void CCombatTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	SetTarget(FindTarget(unit, pos));

	if (GetTarget() == nullptr) {
		RemoveAssignee(unit);
		return;
	}

	if (unit->Blocker() != nullptr) {
		return;  // Do not interrupt current action
	}

	const AIFloat3& position = GetTarget()->GetPos();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float range = std::max(unit->GetUnit()->GetMaxRange(), /*unit->IsUnderWater(frame) ? cdef->GetSonarRadius() : */cdef->GetLosRadius());
	if (position.SqDistance2D(pos) < range) {
		unit->Attack(GetTarget(), GetTarget()->GetUnit()->IsCloaked(), frame + FRAMES_PER_SEC * 60);
	} else {
		const AIFloat3 velLead = GetTarget()->GetVel() * FRAMES_PER_SEC * 3;
		const AIFloat3 lead = velLead.SqLength2D() < SQUARE(300.f)
				? velLead
				: AIFloat3(AIFloat3(GetTarget()->GetVel()).Normalize2D() * 300.f);
		AIFloat3 leadPos = position + lead;
		CTerrainManager::CorrectPosition(leadPos);
		TRY_UNIT(circuit, unit,
			unit->CmdMoveTo(leadPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
	}
}

CEnemyInfo* CCombatTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CMilitaryManager* militaryMgr = circuit->GetMilitaryManager();
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();
	if (pos.SqDistance2D(basePos) > SQUARE(militaryMgr->GetBaseDefRange())) {
		return nullptr;
	}

	CMap* map = circuit->GetMap();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CInfluenceMap* inflMap = circuit->GetInflMap();
	SArea* area = unit->GetArea();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float maxSpeed = SQUARE(cdef->GetSpeed() / FRAMES_PER_SEC);
	const float maxPower = threatMap->GetUnitPower(unit) * powerMod;
	const float weaponRange = cdef->GetMaxRange() * 0.9f;
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float sqCommRadBegin = SQUARE(militaryMgr->GetCommDefRadBegin());
	float minSqDist = SQUARE(militaryMgr->GetCommDefRad(pos.distance2D(basePos)));

	CEnemyInfo* bestTarget = nullptr;
	CEnemyInfo* worstTarget = nullptr;
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyInfos& enemies = circuit->GetEnemyInfos();
	for (auto& kv : enemies) {
		CEnemyInfo* enemy = kv.second;
		// TODO: check how close is another task, and its movement vector
		if (enemy->IsHidden() || (enemy->GetTasks().size() > 1)) {
			continue;
		}

		const AIFloat3& ePos = enemy->GetPos();
		const float sqDist = pos.SqDistance2D(ePos);
		if ((basePos.SqDistance2D(ePos) > sqCommRadBegin) && (sqDist > minSqDist)) {
			continue;
		}

		const float power = threatMap->GetThreatAt(ePos);
		if ((maxPower <= power)
			|| (inflMap->GetAllyDefendInflAt(ePos) < INFL_EPS)
			|| !terrainMgr->CanMoveToPos(area, ePos))
		{
			continue;
		}

		const AIFloat3& eVel = enemy->GetVel();
		if (eVel.SqLength2D() >= maxSpeed) {  // speed
			const AIFloat3 uVec = pos - ePos;
			const float dotProduct = eVel.dot2D(uVec);
			if (dotProduct < 0) {  // direction (angle > 90 deg)
				continue;
			}
			if (dotProduct < SQRT_3_2 * sqrtf(eVel.SqLength2D() * uVec.SqLength2D())) {  // direction (angle > 30 deg)
				continue;
			}
		}

		int targetCat;
		const float elevation = map->GetElevationAt(ePos.x, ePos.z);
		const bool IsInWater = cdef->IsPredictInWater(elevation);
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			targetCat = edef->GetCategory();
			if (((targetCat & canTargetCat) == 0)
				|| (edef->IsAbleToFly() && !(IsInWater ? cdef->HasSubToAir() : cdef->HasSurfToAir())))  // notAA
			{
				continue;
			}
			float elevation = map->GetElevationAt(ePos.x, ePos.z);
			if (edef->IsInWater(elevation, ePos.y)) {
				if (!(IsInWater ? cdef->HasSubToWater() : cdef->HasSurfToWater())) {  // notAW
					continue;
				}
			} else {
				if (!(IsInWater ? cdef->HasSubToLand() : cdef->HasSurfToLand())) {  // notAL
					continue;
				}
			}
			if (ePos.y - elevation > weaponRange) {
				continue;
			}
		} else {
			if (!(IsInWater ? cdef->HasSubToWater() : cdef->HasSurfToWater()) && (ePos.y < -SQUARE_SIZE * 5)) {  // notAW
				continue;
			}
			targetCat = UNKNOWN_CATEGORY;
		}

		if (enemy->IsInRadarOrLOS()) {
			if ((targetCat & noChaseCat) == 0) {
				bestTarget = enemy;
				minSqDist = sqDist;
			} else if (bestTarget == nullptr) {
				worstTarget = enemy;
			}
		}
	}
	if (bestTarget == nullptr) {
		bestTarget = worstTarget;
	}

	return bestTarget;
}

} // namespace circuit
