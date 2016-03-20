/*
 * AllyTeam.cpp
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#include "unit/AllyTeam.h"
#include "unit/FactoryData.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "setup/DefenceMatrix.h"
#include "setup/SetupManager.h"
#include "terrain/PathFinder.h"
#include "terrain/TerrainData.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AIFloat3.h"
#include "OOAICallback.h"
#include "Team.h"

namespace circuit {

using namespace springai;

bool CAllyTeam::SBox::ContainsPoint(const AIFloat3& point) const
{
	return (point.x >= left) && (point.x <= right) &&
		   (point.z >= top) && (point.z <= bottom);
}

CAllyTeam::CAllyTeam(const TeamIds& tids, const SBox& sb)
		: teamIds(tids)
		, startBox(sb)
		, initCount(0)
		, lastUpdate(-1)
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

	int boxId = circuit->GetTeam()->GetRulesParamFloat("start_box_id", -1);
	if (boxId >= 0) {
		startBox = circuit->GetGameAttribute()->GetSetupData().GetStartBox(boxId);
	}

	metalManager = std::make_shared<CMetalManager>(circuit, &circuit->GetGameAttribute()->GetMetalData());
	if (metalManager->HasMetalSpots() && !metalManager->HasMetalClusters() && !metalManager->IsClusterizing()) {
		metalManager->ClusterizeMetal();
	}
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CMetalManager::Init, metalManager));

	energyLink = std::make_shared<CEnergyGrid>(circuit);
	defence = std::make_shared<CDefenceMatrix>(circuit);
	pathfinder = std::make_shared<CPathFinder>(&circuit->GetGameAttribute()->GetTerrainData());
	factoryData = std::make_shared<CFactoryData>(circuit);
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

	metalManager = nullptr;
	energyLink = nullptr;
	defence = nullptr;
	pathfinder = nullptr;
	factoryData = nullptr;
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
	for (Unit* u : units) {
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

CCircuitUnit* CAllyTeam::GetFriendlyUnit(CCircuitUnit::Id unitId) const
{
	decltype(friendlyUnits)::const_iterator it = friendlyUnits.find(unitId);
	return (it != friendlyUnits.end()) ? it->second : nullptr;
}

} // namespace circuit
