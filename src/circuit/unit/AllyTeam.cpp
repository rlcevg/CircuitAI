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
#include "terrain/ThreatMap.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "AIFloat3.h"
#include "OOAICallback.h"
#include "Team.h"
#include "TeamRulesParam.h"

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
		, lastUpdate(-1)
		, initCount(0)
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

	TeamRulesParam* trp = circuit->GetTeam()->GetTeamRulesParamByName("start_box_id");
	if (trp != nullptr) {
		int boxId = trp->GetValueFloat();
		startBox = circuit->GetGameAttribute()->GetSetupData().GetStartBox(boxId);
		delete trp;
	}

	metalManager = std::make_shared<CMetalManager>(circuit, &circuit->GetGameAttribute()->GetMetalData());
	if (metalManager->HasMetalSpots() && !metalManager->HasMetalClusters() && !metalManager->IsClusterizing()) {
		metalManager->ClusterizeMetal();
	}
	// Init after parallel clusterization
	circuit->GetScheduler()->RunParallelTask(CGameTask::emptyTask, std::make_shared<CGameTask>(&CMetalManager::Init, metalManager));
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

CCircuitUnit* CAllyTeam::GetFriendlyUnit(CCircuitUnit::Id unitId) const
{
	decltype(friendlyUnits)::const_iterator it = friendlyUnits.find(unitId);
	return (it != friendlyUnits.end()) ? it->second : nullptr;
}

} // namespace circuit
