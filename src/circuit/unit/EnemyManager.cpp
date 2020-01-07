/*
 * EnemyManager.cpp
 *
 *  Created on: Dec 25, 2019
 *      Author: rlcevg
 */

#include "unit/EnemyManager.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
// FIXME: DEBUG
#include "map/ThreatMap.h"
// FIXME: DEBUG

#include "WrappUnit.h"
#include "Cheats.h"

namespace circuit {

using namespace springai;

CEnemyManager::CEnemyManager(CCircuitAI* circuit)
		: circuit(circuit)
		, enemyIterator(0)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
//	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CMapManager::EnqueueUpdate, this), THREAT_UPDATE_RATE, circuit->GetSkirmishAIId());
}

CEnemyManager::~CEnemyManager()
{
	for (auto& kv : enemyUnits) {
		delete kv.second;
	}
}

CEnemyUnit* CEnemyManager::GetEnemyUnit(ICoreUnit::Id unitId) const
{
	auto it = enemyUnits.find(unitId);
	return (it != enemyUnits.end()) ? it->second : nullptr;
}

std::pair<CEnemyUnit*, bool> CEnemyManager::RegisterEnemyUnit(ICoreUnit::Id unitId, bool isInLOS, CCircuitAI* ai)
{
	CEnemyUnit* data = GetEnemyUnit(unitId);
	if (circuit != ai) {
		return std::make_pair(data, true);
	}

	Unit* u = WrappUnit::GetInstance(circuit->GetSkirmishAIId(), unitId);
	if (u == nullptr) {
		return std::make_pair(nullptr, true);
	}
	if (/*u->IsNeutral() || */u->GetRulesParamFloat("ignoredByAI", 0.f) > 0.f) {
		delete u;
		return std::make_pair(nullptr, false);
	}
	CCircuitDef* cdef = nullptr;
	if (isInLOS) {
		UnitDef* unitDef = u->GetDef();
		if (unitDef == nullptr) {  // doesn't work with globalLOS
			delete u;
			return std::make_pair(nullptr, false);
		}
		cdef = circuit->GetCircuitDef(unitDef->GetUnitDefId());
		delete unitDef;
	}
	data = new CEnemyUnit(unitId, u, cdef);

	enemyUnits[data->GetId()] = data;
	enemyUpdates.push_back(data);

	return std::make_pair(data, true);
}

CEnemyUnit* CEnemyManager::RegisterEnemyUnit(Unit* e, CCircuitAI* ai)
{
	if (circuit != ai) {
		return GetEnemyUnit(e->GetUnitId());
	}

	if (/*e->IsNeutral() || */e->GetRulesParamFloat("ignoredByAI", 0.f) > 0.f) {
		return nullptr;
	}

	const ICoreUnit::Id unitId = e->GetUnitId();
	CEnemyUnit* data = GetEnemyUnit(unitId);
	UnitDef* unitDef = e->GetDef();
	CCircuitDef::Id unitDefId = unitDef->GetUnitDefId();
	delete unitDef;

	if (data != nullptr) {
		if ((data->GetCircuitDef() == nullptr) || data->GetCircuitDef()->GetId() != unitDefId) {
			data->SetCircuitDef(circuit->GetCircuitDef(unitDefId));
			data->SetCost(data->GetUnit()->GetRulesParamFloat("comm_cost", data->GetCost()));
		}
		return nullptr;
	}

	CCircuitDef* cdef = circuit->GetCircuitDef(unitDefId);
	data = new CEnemyUnit(unitId, e, cdef);

	enemyUnits[data->GetId()] = data;
	enemyUpdates.push_back(data);

	return data;
}

bool CEnemyManager::UnitInLOS(CEnemyUnit* data, CCircuitAI* ai)
{
	if (circuit != ai) {
		return true;
	}

	UnitDef* unitDef = data->GetUnit()->GetDef();
	if (unitDef == nullptr) {  // doesn't work with globalLOS
		return false;
	}
	CCircuitDef::Id unitDefId = unitDef->GetUnitDefId();
	delete unitDef;
	if ((data->GetCircuitDef() == nullptr) || data->GetCircuitDef()->GetId() != unitDefId) {
		data->SetCircuitDef(circuit->GetCircuitDef(unitDefId));
		data->SetCost(data->GetUnit()->GetRulesParamFloat("comm_cost", data->GetCost()));
	}
	return true;
}

void CEnemyManager::UnregisterEnemyUnit(CEnemyUnit* data, CCircuitAI* ai)
{
	if (circuit != ai) {
		return;
	}

	UnregisterEnemyUnit(data);
}

void CEnemyManager::UnregisterEnemyUnit(CEnemyUnit* data)
{
	enemyUnits.erase(data->GetId());
	data->Dead();
}

void CEnemyManager::DeleteEnemyUnit(CEnemyUnit* data)
{
	enemyUpdates[enemyIterator] = enemyUpdates.back();
	enemyUpdates.pop_back();

	delete data;
}

void CEnemyManager::GarbageEnemy(CEnemyUnit* enemy)
{
	enemyGarbage.push_back(enemy->GetId());
	UnregisterEnemyUnit(enemy);
	++enemyIterator;
}

void CEnemyManager::UpdateEnemyDatas()
{
	if (!circuit->IsCheating()) {
		// AI knows what units are in los, hence reduce the amount of useless
		// engine InLos checks for each single param of the enemy unit
		circuit->GetCheats()->SetEnabled(true);
	}

	if (enemyIterator >= enemyUpdates.size()) {
		enemyIterator = 0;
	}
	enemyGarbage.clear();

	// stagger the Update's
	// -2 is for threat-draw frame and k-means frame
	unsigned int n = (enemyUpdates.size() / (THREAT_UPDATE_RATE - 2)) + 1;

	const int maxFrame = circuit->GetLastFrame() - FRAMES_PER_SEC * 60 * 20;
	while ((enemyIterator < enemyUpdates.size()) && (n != 0)) {
		CEnemyUnit* enemy = enemyUpdates[enemyIterator];
		if (enemy->IsDead()) {
			DeleteEnemyUnit(enemy);
			continue;
		}

		int frame = enemy->GetLastSeen();
		if ((frame != -1) && (maxFrame >= frame)) {
			GarbageEnemy(enemy);
			continue;
		}

		if (enemy->IsInRadarOrLOS()) {
			const AIFloat3& pos = enemy->GetUnit()->GetPos();
			if (CTerrainData::IsNotInBounds(pos)) {  // NOTE: Unit id validation. No EnemyDestroyed sometimes apparently
				GarbageEnemy(enemy);
				continue;
			}
			enemy->UpdateInRadarData(pos);
			if (enemy->IsInLOS()) {
				enemy->UpdateInLosData();  // heavy on engine calls
			}
		}

		++enemyIterator;
		--n;
	}

	if (!circuit->IsCheating()) {
		circuit->GetCheats()->SetEnabled(false);
	}
}

} // namespace circuit
