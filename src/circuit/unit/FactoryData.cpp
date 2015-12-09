/*
 * FactoryData.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: rlcevg
 */

#include "unit/FactoryData.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/utils.h"
#include "json/json.h"

#include <algorithm>

namespace circuit {

CFactoryData::CFactoryData(CCircuitAI *circuit)
		: isFirstChoice(true)
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& factories = root["factories"];

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

		// TODO: Replace importance with proper terrain analysis (size, hardness, unit's power, speed)
		const Json::Value& importance = factories[fac]["importance"];
		if (importance == Json::Value::null) {
			sfac.startImp = 1.0f;
			sfac.switchImp = 1.0f;
		} else {
			sfac.startImp = importance.get((unsigned)0, 1.0f).asFloat();
			sfac.switchImp = importance.get((unsigned)1, 1.0f).asFloat();
		}

		if (cdef->GetMobileId() < 0) {  // air
			sfac.offset = (float)rand() / RAND_MAX * 50.0;
		} else {
			sfac.offset = (float)rand() / RAND_MAX * 40.0 - 20.0;
		}

		allFactories[sfac.id] = sfac;
	}
}

CFactoryData::~CFactoryData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

CCircuitDef* CFactoryData::GetFactoryToBuild(CCircuitAI* circuit, bool isStart)
{
	std::vector<SFactory> availFacs;
	std::map<CCircuitDef::Id, float> percents;
	SAreaData* areaData = circuit->GetTerrainManager()->GetAreaData();
	const std::vector<STerrainMapImmobileType>& immobileType = areaData->immobileType;
	const std::vector<STerrainMapMobileType>& mobileType = areaData->mobileType;
	for (const auto& kv : allFactories) {
		const SFactory& sfac = kv.second;
		CCircuitDef* cdef = circuit->GetCircuitDef(sfac.id);
		if (!cdef->IsAvailable()) {
			continue;
		}
		STerrainMapImmobileType::Id itId = cdef->GetImmobileId();
		if (!immobileType[itId].typeUsable) {
			continue;
		}
		const float importance = isStart ? sfac.startImp : sfac.switchImp;
		if (importance <= .0f) {
			continue;
		}

		STerrainMapMobileType::Id mtId = cdef->GetMobileId();
		if (mtId < 0) {
			availFacs.push_back(sfac);
			percents[sfac.id] = sfac.offset + importance * 60.0;
		} else if (mobileType[mtId].typeUsable) {
			availFacs.push_back(sfac);
			percents[sfac.id] = sfac.offset + importance * mobileType[mtId].areaLargest->percentOfMap;
		}
	}

	if (availFacs.empty()) {
		return nullptr;
	}

	auto cmp = [circuit, &percents](const SFactory& a, const SFactory& b) {
		if (a.count > b.count) {
			return false;
		}
		return percents[a.id] > percents[b.id];
	};
	std::sort(availFacs.begin(), availFacs.end(), cmp);

	// Don't start with air
	if (isFirstChoice && (circuit->GetCircuitDef(availFacs.front().id)->GetMobileId() < 0)) {
		for (SFactory& fac : availFacs) {
			if (circuit->GetCircuitDef(fac.id)->GetMobileId() >= 0) {
				std::swap(availFacs.front(), fac);
				break;
			}
		}
	}
	isFirstChoice = false;

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
