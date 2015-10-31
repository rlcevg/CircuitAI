/*
 * AllyTeam.cpp
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#include "unit/AllyTeam.h"
#include "resource/MetalManager.h"
#include "resource/EnergyGrid.h"
#include "setup/DefenceMatrix.h"
#include "terrain/PathFinder.h"
#include "terrain/TerrainData.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
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
		, initCount(0)
		, lastUpdate(-1)
		, factoryIdx(0)
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
	defence = std::make_shared<CDefenceMatrix>(circuit);
	pathfinder = std::make_shared<CPathFinder>(&circuit->GetGameAttribute()->GetTerrainData());

	const char* factories[] = {
		"factorycloak",
		"factoryamph",
		"factoryhover",
		"factoryjump",
		"factoryshield",
		"factoryspider",
		"factorytank",
		"factoryveh",
		"factoryplane",
		"factorygunship",
		"factoryship",
	};
	const int size = sizeof(factories) / sizeof(factories[0]);
	factoryBuilds.reserve(size);
	std::map<STerrainMapMobileType::Id, float> percents;
	CTerrainData& terrainData = circuit->GetGameAttribute()->GetTerrainData();
	const std::vector<STerrainMapImmobileType>& immobileType = terrainData.areaData0.immobileType;
	const std::vector<STerrainMapMobileType>& mobileType = terrainData.areaData0.mobileType;
	for (const char* fac : factories) {
		CCircuitDef* cdef = circuit->GetCircuitDef(fac);
		STerrainMapImmobileType::Id itId = cdef->GetImmobileId();
		if ((itId < 0) || !immobileType[itId].typeUsable) {
			continue;
		}

		STerrainMapMobileType::Id mtId = cdef->GetMobileId();
		if (mtId < 0) {
			factoryBuilds.push_back(cdef->GetId());
			percents[cdef->GetId()] = 50.0 + rand() / (float)RAND_MAX * 50.0;
		} else if (mobileType[mtId].typeUsable) {
			factoryBuilds.push_back(cdef->GetId());
			float shift = rand() / (float)RAND_MAX * 40.0 - 20.0;
			percents[cdef->GetId()] = mobileType[mtId].areaLargest->percentOfMap + shift;
		}
	}
	auto cmp = [circuit, &percents](const CCircuitDef::Id aId, const CCircuitDef::Id bId) {
		return percents[aId] > percents[bId];
	};
	std::sort(factoryBuilds.begin(), factoryBuilds.end(), cmp);
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

CCircuitDef* CAllyTeam::GetFactoryToBuild(CCircuitAI* circuit)
{
	for (int i = 0; i < factoryBuilds.size(); ++i) {
		CCircuitDef* cdef = circuit->GetCircuitDef(factoryBuilds[factoryIdx]);
		if (cdef->IsAvailable()) {
			return cdef;
		}
		AdvanceFactoryIdx();
	}
	return nullptr;
}

} // namespace circuit
