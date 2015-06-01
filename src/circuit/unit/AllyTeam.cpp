/*
 * AllyTeam.cpp
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#include "unit/AllyTeam.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "static/GameAttribute.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AIFloat3.h"
#include "OOAICallback.h"

namespace circuit {

using namespace springai;

bool CAllyTeam::SBox::ContainsPoint(const AIFloat3& point) const
{
	return (point.x >= left) && (point.x <= right) &&
		   (point.z >= top) && (point.z <= bottom);
}

CAllyTeam::CAllyTeam(const TeamIds& tids, const SBox& sb) :
		teamIds(tids),
		startBox(sb),
		lastUpdate(-1),
		initCount(0),
		energyLink(nullptr)
{
}

CAllyTeam::~CAllyTeam()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	if (initCount > 0) {
		initCount = 1;
		Release();
	}
}

void CAllyTeam::Init(CCircuitAI* circuit)
{
	if (initCount++ > 0) {
		return;
	}

	metalManager = std::make_shared<CMetalManager>(circuit, &circuit->GetGameAttribute()->GetMetalData());
	if (metalManager->HasMetalSpots() && !metalManager->HasMetalClusters() && !metalManager->IsClusterizing()) {
		metalManager->ClusterizeMetal();
	}

	energyLink = std::make_shared<CEnergyGrid>(circuit);
}

void CAllyTeam::Release()
{
	if (--initCount > 0) {
		return;
	}

	for (auto& kv : friendlyUnits) {
		delete kv.second;
	}
	friendlyUnits.clear();
	for (auto& kv : enemyUnits) {
		delete kv.second;
	}
	enemyUnits.clear();

	metalManager = nullptr;
	energyLink = nullptr;
}

void CAllyTeam::UpdateFriendlyUnits(CCircuitAI* circuit)
{
	if (lastUpdate >= circuit->GetLastFrame()) {
		return;
	}

	for (auto& kv : friendlyUnits) {
		delete kv.second;
	}
	friendlyUnits.clear();
	const std::vector<Unit*>& units = circuit->GetCallback()->GetFriendlyUnits();
	for (auto u : units) {
		// FIXME: Why engine returns vector with some nullptrs?
		// TODO: Check every GetEnemy/FriendlyUnits for nullptr
		if (u == nullptr) {
			continue;
		}
		int unitId = u->GetUnitId();
		UnitDef* unitDef = u->GetDef();
		CCircuitUnit* unit = new CCircuitUnit(u, circuit->GetCircuitDef(unitDef->GetUnitDefId()));
		delete unitDef;
		friendlyUnits[unitId] = unit;
	}
	lastUpdate = circuit->GetLastFrame();
}

CCircuitUnit* CAllyTeam::GetFriendlyUnit(CCircuitUnit::Id unitId)
{
	decltype(friendlyUnits)::iterator it = friendlyUnits.find(unitId);
	return (it != friendlyUnits.end()) ? it->second : nullptr;
}

CCircuitUnit* CAllyTeam::GetEnemyUnit(CCircuitUnit::Id unitId)
{
	decltype(enemyUnits)::iterator it = enemyUnits.find(unitId);
	return (it != enemyUnits.end()) ? it->second : nullptr;
}

} // namespace circuit
