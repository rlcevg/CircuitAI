/*
 * FactoryData.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: rlcevg
 */

#include "unit/FactoryData.h"
#include "module/FactoryManager.h"
#include "module/BuilderManager.h"  // FIXME: only for IsBuilderInArea
#include "module/EconomyManager.h"  // FIXME: only for IsFactoryDefAvail
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"
#include "json/json.h"

#include <algorithm>

namespace circuit {

using namespace springai;
using namespace terrain;

CFactoryData::CFactoryData()
		: choiceNum(0)
{
}

CFactoryData::~CFactoryData()
{
}

CCircuitDef* CFactoryData::GetFactoryToBuild(CCircuitAI* circuit, AIFloat3 position,
											 bool isStart, bool isReset)
{
	std::vector<CCircuitDef::Id> availFacs;
	std::map<CCircuitDef::Id, float> percents;
	CFactoryManager* factoryMgr = circuit->GetFactoryManager();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	SAreaData* areaData = terrainMgr->GetAreaData();
	const std::vector<SImmobileType>& immobileType = areaData->immobileType;
	const std::vector<SMobileType>& mobileType = areaData->mobileType;
	const bool isAirValid = circuit->GetEnemyManager()->IsAirValid();

	std::function<bool (CCircuitDef*)> predicate;
	bool isPosValid = geom::is_valid(position);
//	CTerrainManager::CorrectPosition(position);
	if (isPosValid) {
		CBuilderManager* builderMgr = circuit->GetBuilderManager();
		predicate = [position, builderMgr, terrainMgr, factoryMgr](CCircuitDef* cdef) {
			if (!builderMgr->IsBuilderInArea(cdef, position) || !terrainMgr->CanBeBuiltAtSafe(cdef, position)) {
				return false;
			}
			CCircuitDef* reprDef = factoryMgr->GetRepresenter(cdef);
			if (reprDef == nullptr) {
				return true;
			}
			return terrainMgr->CanBeBuiltAt(reprDef, position);
		};
	} else {
		// FIXME: Make position being valid - a requirement.
		//        As CBFactoryTask::FindBuildSite may fail
		//        with terrainMgr->CanBeBuiltAt(reprDef, p) condition.
		CEconomyManager* economyMgr = circuit->GetEconomyManager();
		predicate = [economyMgr](CCircuitDef* cdef) {
			return economyMgr->IsFactoryDefAvail(cdef);
		};
	}
	const CFactoryManager::FactoryDefs& factoryDefs = factoryMgr->GetFactoryDefs();
	const float lenOffset = factoryMgr->GetLenOffset();
	const float minOffset = factoryMgr->GetMinOffset();
	const float airMapPerc = factoryMgr->GetAirMapPerc();

	const int frame = circuit->GetLastFrame();
	for (const auto& kv : factoryDefs) {
		const CFactoryManager::SFactoryDef& sfac = kv.second;
		const CCircuitDef::Id defId = kv.first;
		CCircuitDef* cdef = circuit->GetCircuitDef(defId);
		if (!cdef->IsAvailable(frame) ||
			!immobileType[cdef->GetImmobileId()].typeUsable ||
//			(buildableFacs.find(sfac.id) == buildableFacs.end()) ||
			!predicate(cdef))
		{
			continue;
		}
		float importance;
		if (isStart) {
			CCircuitDef* bdef = factoryMgr->GetRoleDef(cdef, ROLE_TYPE(BUILDER));
			importance = sfac.startImp * (((bdef != nullptr) && bdef->IsAvailable(frame)) ? 1.f : .1f);
		} else {
			importance = sfac.switchImp;
		}
		if (importance <= .0f) {
			continue;
		}

		SMobileType::Id mtId = cdef->GetMobileId();
		if (((mtId < 0) && isAirValid) ||
			((mtId >= 0) && mobileType[mtId].typeUsable))
		{
			availFacs.push_back(defId);
			const float offset = (float)rand() / RAND_MAX * lenOffset + minOffset;
			const float speedPercent = sfac.mapSpeedPerc;
			const float mapPercent = (mtId < 0) ? airMapPerc : mobileType[mtId].areaLargest->percentOfMap;
			percents[defId] = offset + importance * (mapPercent + speedPercent);
		}
	}

	if (availFacs.empty()) {
		return nullptr;
	}

	auto cmp = [this, &percents](const CCircuitDef::Id aId, const CCircuitDef::Id bId) {
		if (allFactories[aId].count < allFactories[bId].count) {
			return true;
		} else if (allFactories[aId].count > allFactories[bId].count) {
			return false;
		}
		return percents[aId] > percents[bId];
	};
	std::sort(availFacs.begin(), availFacs.end(), cmp);
#if 0
	for (CCircuitDef::Id facId : availFacs) {
		circuit->LOG("%s | %f", circuit->GetCircuitDef(facId)->GetDef()->GetName(), percents[facId]);
	}
#endif

	if (isReset) {
		choiceNum = 0;
	}
	const unsigned int noAirNum = factoryMgr->GetNoAirNum();

	// Don't start with air
	if (((choiceNum++ < noAirNum)/* || (isPosValid && terrainMgr->IsWaterSector(position))*/)
		&& (circuit->GetCircuitDef(availFacs.front())->GetMobileId() < 0))
	{
		for (CCircuitDef::Id facId : availFacs) {
			if (circuit->GetCircuitDef(facId)->GetMobileId() >= 0) {
				std::swap(availFacs.front(), facId);
				break;
			}
		}
	}

	return circuit->GetCircuitDef(availFacs.front());
}

void CFactoryData::AddFactory(const CCircuitDef* cdef)
{
	++allFactories[cdef->GetId()].count;
}

void CFactoryData::DelFactory(const CCircuitDef* cdef)
{
	auto it = allFactories.find(cdef->GetId());
	if (it != allFactories.end()) {
		--it->second.count;
	}
}

} // namespace circuit
