/*
 * BuilderTask.cpp
 *
 *  Created on: Apr 10, 2020
 *      Author: rlcevg
 */

#include "task/fighter/BuilderTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBuilderTask::CBuilderTask(ITaskManager* mgr, float powerMod)
		: IFighterTask(mgr, FightType::DEFEND, powerMod)
{
}

CBuilderTask::~CBuilderTask()
{
}

void CBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CBuilderTask::Start(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CBuilderTask::Update()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();

	decltype(units) tmpUnits = units;
	if ((++updCount % 2 == 0) && (frame >= lastTouched + FRAMES_PER_SEC)) {
		lastTouched = frame;
		for (CCircuitUnit* unit : tmpUnits) {
			Execute(unit, true);
		}
	} else {
		for (CCircuitUnit* unit : tmpUnits) {
			if (unit->IsForceExecute(frame)) {
				Execute(unit, true);
			}
		}
	}
}

void CBuilderTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	SetTarget(nullptr);  // make adequate enemy->GetTasks().size()
	SetTarget(FindTarget(unit, pos));

	if (target == nullptr) {
		CBuilderManager* builderManager = circuit->GetBuilderManager();
		RemoveAssignee(unit);
		unit->GetTask()->RemoveAssignee(unit);
		builderManager->UnitFinished(unit);
		return;
	}

	position = target->GetPos();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float range = std::max(unit->GetUnit()->GetMaxRange(), /*unit->IsUnderWater(frame) ? cdef->GetSonarRadius() : */cdef->GetLosRadius());
	if (position.SqDistance2D(pos) < range) {
		if (target->GetUnit()->IsCloaked()) {
			TRY_UNIT(circuit, unit,
				unit->GetUnit()->ExecuteCustomCommand(CMD_ATTACK_GROUND, {position.x, position.y, position.z},
													  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			)
		} else {
			unit->Attack(target, frame + FRAMES_PER_SEC * 60);
		}
	} else {
		TRY_UNIT(circuit, unit,
			unit->GetUnit()->MoveTo(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		)
	}
}

CEnemyInfo* CBuilderTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CMap* map = circuit->GetMap();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& basePos = circuit->GetSetupManager()->GetBasePos();
	STerrainMapArea* area = unit->GetArea();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const bool notAW = !cdef->HasAntiWater();
	const bool notAA = !cdef->HasAntiAir();
	const float maxPower = threatMap->GetUnitThreat(unit) * powerMod;
	const float weaponRange = cdef->GetMaxRange();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	float minSqDist = SQUARE(2000.f);

	CEnemyInfo* bestTarget = nullptr;
	CEnemyInfo* worstTarget = nullptr;
	threatMap->SetThreatType(unit);
	const CCircuitAI::EnemyInfos& enemies = circuit->GetEnemyInfos();
	for (auto& kv : enemies) {
		CEnemyInfo* enemy = kv.second;
		if (enemy->IsHidden()) {
			continue;
		}
		const AIFloat3& ePos = enemy->GetPos();

		if (basePos.SqDistance2D(ePos) > SQUARE(2000.f)) {
			continue;
		}

		const float power = threatMap->GetThreatAt(ePos);
		if ((maxPower <= power)
			|| !terrainManager->CanMoveToPos(area, ePos))
		{
			continue;
		}

		int targetCat;
		CCircuitDef* edef = enemy->GetCircuitDef();
		if (edef != nullptr) {
			targetCat = edef->GetCategory();
			if (((targetCat & canTargetCat) == 0)
				|| (edef->IsAbleToFly() && notAA))
			{
				continue;
			}
			float elevation = map->GetElevationAt(ePos.x, ePos.z);
			if ((notAW && !edef->IsYTargetable(elevation, ePos.y))
				|| (ePos.y - elevation > weaponRange))
			{
				continue;
			}
		} else {
			if (notAW && (ePos.y < -SQUARE_SIZE * 5)) {
				continue;
			}
			targetCat = UNKNOWN_CATEGORY;
		}

		float sqDist = pos.SqDistance2D(ePos);
		if (minSqDist > sqDist) {
			if (enemy->IsInRadarOrLOS()) {
				if ((targetCat & noChaseCat) == 0) {
					bestTarget = enemy;
					minSqDist = sqDist;
				} else if (bestTarget == nullptr) {
					worstTarget = enemy;
				}
			}
		}
	}
	if (bestTarget == nullptr) {
		bestTarget = worstTarget;
	}

	if (bestTarget != nullptr) {
		return bestTarget;
	}

	return nullptr;
}

} // namespace circuit
