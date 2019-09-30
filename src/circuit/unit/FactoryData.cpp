/*
 * FactoryData.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: rlcevg
 */

#include "unit/FactoryData.h"
#include "module/FactoryManager.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"
#include "json/json.h"

#include "Map.h"

#include <algorithm>

namespace circuit {

using namespace springai;

CFactoryData::CFactoryData(CCircuitAI *circuit)
		: choiceNum(0)
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& factories = root["factory"];

	float minSpeed = std::numeric_limits<float>::max();
	float maxSpeed = 0.f;

	for (const std::string& fac : factories.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(fac.c_str());
		if (cdef == nullptr) {
			continue;
		}
		STerrainMapImmobileType::Id itId = cdef->GetImmobileId();
		if (itId < 0) {  // didn't identify proper type?
			continue;
		}

		SFactory sfac;
		sfac.id = cdef->GetId();
		sfac.count = 0;

		const Json::Value& factory = factories[fac];
		// TODO: Replace importance with proper terrain analysis (size, hardness, unit's power, speed)
		const Json::Value& importance = factory["importance"];
		if (importance.isNull()) {
			sfac.startImp = 1.0f;
			sfac.switchImp = 1.0f;
		} else {
			sfac.startImp = importance.get((unsigned)0, 1.0f).asFloat();
			sfac.switchImp = importance.get((unsigned)1, 1.0f).asFloat();
		}

		const Json::Value& items = factory["unit"];
		unsigned size = 0;
		float avgSpeed = 0.f;
		for (const Json::Value& item : items) {
			CCircuitDef* udef = circuit->GetCircuitDef(item.asCString());
			if (udef != nullptr) {
				avgSpeed += udef->GetSpeed();
				++size;
			}
		}
		if (size > 0) {
			avgSpeed /= size;
		}
		minSpeed = std::min(minSpeed, avgSpeed);
		maxSpeed = std::max(maxSpeed, avgSpeed);
		sfac.mapSpeedPerc = avgSpeed;

		allFactories[sfac.id] = sfac;
	}

	const Json::Value& select = root["select"];
	const Json::Value& offset = select["offset"];
	const Json::Value& speed = select["speed"];
	const Json::Value& map = select["map"];
	airMapPerc = select.get("air_map", 80.0f).asFloat();
	minOffset = offset.get((unsigned)0, -20.0f).asFloat();
	const float maxOffset = offset.get((unsigned)1, 20.0f).asFloat();
	lenOffset = maxOffset - minOffset;
	const float minSpPerc = speed.get((unsigned)0, 0.0f).asFloat();
	const float maxSpPerc = speed.get((unsigned)1, 40.0f).asFloat();
	const float minMap = map.get((unsigned)0, 8.0f).asFloat();
	const float maxMap = map.get((unsigned)1, 24.0f).asFloat();
	const float minMapSp = SQUARE(minMap) * minSpeed;
	const float mapSize = (circuit->GetMap()->GetWidth() / 64) * (circuit->GetMap()->GetHeight() / 64);
	const float speedFactor = (maxSpPerc - minSpPerc) / (SQUARE(maxMap) * maxSpeed - minMapSp);
	for (auto& kv : allFactories) {
		float avgSpeed = kv.second.mapSpeedPerc;
		kv.second.mapSpeedPerc = speedFactor * (mapSize * avgSpeed - minMapSp) + minSpPerc;
	}

	noAirNum = select.get("no_air", 2).asUInt();
}

CFactoryData::~CFactoryData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

CCircuitDef* CFactoryData::GetFactoryToBuild(CCircuitAI* circuit, AIFloat3 position,
											 bool isStart, bool isReset)
{
	std::vector<SFactory> availFacs;
	std::map<CCircuitDef::Id, float> percents;
	CFactoryManager* factoryManager = circuit->GetFactoryManager();
	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	SAreaData* areaData = terrainManager->GetAreaData();
	const std::vector<STerrainMapImmobileType>& immobileType = areaData->immobileType;
	const std::vector<STerrainMapMobileType>& mobileType = areaData->mobileType;
	const bool isAirValid = circuit->GetMilitaryManager()->IsAirValid();

	std::function<bool (CCircuitDef*)> predicate;
	bool isPosValid = utils::is_valid(position);
//	CTerrainManager::CorrectPosition(position);
	if (isPosValid) {
		predicate = [position, terrainManager](CCircuitDef* cdef) {
			return terrainManager->CanBeBuiltAtSafe(cdef, position);
		};
	} else {
		predicate = [](CCircuitDef* cdef) {
			return true;
		};
	}

	const int frame = circuit->GetLastFrame();
	for (const auto& kv : allFactories) {
		const SFactory& sfac = kv.second;
		CCircuitDef* cdef = circuit->GetCircuitDef(sfac.id);
		if (!cdef->IsAvailable(frame) ||
			!immobileType[cdef->GetImmobileId()].typeUsable ||
			!predicate(cdef))
		{
			continue;
		}
		float importance;
		if (isStart) {
			CCircuitDef* bdef = factoryManager->GetRoleDef(cdef, CCircuitDef::RoleType::BUILDER);
			importance = sfac.startImp * (((bdef != nullptr) && bdef->IsAvailable()) ? 1.f : .1f);
		} else {
			importance = sfac.switchImp;
		}
		if (importance <= .0f) {
			continue;
		}

		STerrainMapMobileType::Id mtId = cdef->GetMobileId();
		if (((mtId < 0) && isAirValid) ||
			((mtId >= 0) && mobileType[mtId].typeUsable))
		{
			availFacs.push_back(sfac);
			const float offset = (float)rand() / RAND_MAX * lenOffset + minOffset;
			const float speedPercent = sfac.mapSpeedPerc;
			const float mapPercent = (mtId < 0) ? airMapPerc : mobileType[mtId].areaLargest->percentOfMap;
			percents[sfac.id] = offset + importance * (mapPercent + speedPercent);
		}
	}

	if (availFacs.empty()) {
		return nullptr;
	}

	auto cmp = [&percents](const SFactory& a, const SFactory& b) {
		if (a.count < b.count) {
			return true;
		} else if (a.count > b.count) {
			return false;
		}
		return percents[a.id] > percents[b.id];
	};
	std::sort(availFacs.begin(), availFacs.end(), cmp);

	if (isReset) {
		choiceNum = 0;
	}

	// Don't start with air
	if (((choiceNum++ < noAirNum) || (isPosValid && terrainManager->IsWaterSector(position))) &&
		(circuit->GetCircuitDef(availFacs.front().id)->GetMobileId() < 0))
	{
		for (SFactory& fac : availFacs) {
			if (circuit->GetCircuitDef(fac.id)->GetMobileId() >= 0) {
				std::swap(availFacs.front(), fac);
				break;
			}
		}
	}

	return circuit->GetCircuitDef(availFacs.front().id);
}

void CFactoryData::AddFactory(CCircuitDef* cdef)
{
	auto it = allFactories.find(cdef->GetId());
	if (it != allFactories.end()) {
		++it->second.count;
	}
}

void CFactoryData::DelFactory(CCircuitDef* cdef)
{
	auto it = allFactories.find(cdef->GetId());
	if (it != allFactories.end()) {
		--it->second.count;
	}
}

} // namespace circuit
