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

int CAllyTeam::GetSize() const
{
	return teamIds.size();
}

const CAllyTeam::TeamIds& CAllyTeam::GetTeamIds() const
{
	return teamIds;
}

const CAllyTeam::SBox& CAllyTeam::GetStartBox() const
{
	return startBox;
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
	int frame = circuit->GetLastFrame();
	if (lastUpdate >= frame) {
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
	lastUpdate = frame;
}

CCircuitUnit* CAllyTeam::GetFriendlyUnit(CCircuitUnit::Id unitId)
{
	decltype(friendlyUnits)::iterator i = friendlyUnits.find(unitId);
	if (i != friendlyUnits.end()) {
		return i->second;
	}

	return nullptr;
}

const CAllyTeam::Units& CAllyTeam::GetFriendlyUnits() const
{
	return friendlyUnits;
}

void CAllyTeam::AddEnemyUnit(CCircuitUnit* unit)
{
	enemyUnits[unit->GetId()] = unit;
}

void CAllyTeam::RemoveEnemyUnit(CCircuitUnit* unit)
{
	enemyUnits.erase(unit->GetId());
}

CCircuitUnit* CAllyTeam::GetEnemyUnit(CCircuitUnit::Id unitId)
{
	decltype(enemyUnits)::iterator i = enemyUnits.find(unitId);
	if (i != enemyUnits.end()) {
		return i->second;
	}

	return nullptr;
}

const CAllyTeam::Units& CAllyTeam::GetEnemyUnits() const
{
	return enemyUnits;
}

std::shared_ptr<CMetalManager>& CAllyTeam::GetMetalManager()
{
	return metalManager;
}

std::shared_ptr<CEnergyGrid>& CAllyTeam::GetEnergyLink()
{
	return energyLink;
}

} // namespace circuit
