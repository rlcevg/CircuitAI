/*
 * ScoutTask.cpp
 *
 *  Created on: Jan 28, 2015
 *      Author: rlcevg
 */

#include "task/fighter/ScoutTask.h"
#include "task/TaskManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "resource/MetalManager.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CScoutTask::CScoutTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::SCOUT)
{
}

CScoutTask::~CScoutTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CScoutTask::Execute(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	Unit* u = unit->GetUnit();

	const AIFloat3& pos = u->GetPos();
	STerrainMapArea* area = unit->GetArea();
	float power = threatMap->GetUnitThreat(unit) * 0.5f;
	CEnemyUnit* bestTarget = nullptr;
	float minSqDist = std::numeric_limits<float>::max();
	const CCircuitAI::EnemyUnits& enemies = threatMap->GetPeaceUnits();
	for (auto& kv : enemies) {
		CEnemyUnit* enemy = kv.second;
		if (enemy->IsHidden() || (threatMap->GetThreatAt(enemy->GetPos()) >= power) ||
			!terrainManager->CanMoveToPos(area, enemy->GetPos()))
		{
			continue;
		}
		float sqDist = pos.SqDistance2D(enemy->GetPos());
		if (sqDist < minSqDist) {
			bestTarget = enemy;
			minSqDist = sqDist;
		}
	}

	if (bestTarget != nullptr) {
		position = bestTarget->GetPos();
		float range = u->GetMaxRange();
		if ((bestTarget->GetCircuitDef() != nullptr) && (minSqDist < range * range)) {
			int targetCat = bestTarget->GetCircuitDef()->GetUnitDef()->GetCategory();
			int noChaseCat = unit->GetCircuitDef()->GetUnitDef()->GetNoChaseCategory();
			if (targetCat & noChaseCat != 0) {
				u->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
				return;
			}
		}
		u->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
		return;
	}

	CMetalManager* metalManager = circuit->GetMetalManager();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	if (!clusters.empty()) {
		int index = rand() % clusters.size();
		position = clusters[index].geoCentr;
		const CMetalData::Metals& spots = metalManager->GetSpots();
		auto it = clusters[index].idxSpots.begin();
		u->Fight(spots[*it].position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
		auto end = clusters[index].idxSpots.end();
		while (++it != end) {
			u->Fight(spots[*it].position, UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 300);
		}
		return;
	}

	float x = rand() % (terrainManager->GetTerrainWidth() + 1);
	float z = rand() % (terrainManager->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	u->Fight(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 300);
}

void CScoutTask::Update()
{
	IFighterTask::Update();

	CThreatMap* threatMap = manager->GetCircuit()->GetThreatMap();
	for (CCircuitUnit* unit : units) {
		float power = threatMap->GetUnitThreat(unit) * 0.5f;
		if (threatMap->GetThreatAt(position) >= power) {
			manager->AbortTask(this);
			break;
		}
	}
}

} // namespace circuit
