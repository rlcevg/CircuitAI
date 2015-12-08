/*
 * FactoryData.cpp
 *
 *  Created on: Dec 8, 2015
 *      Author: rlcevg
 */

#include "unit/FactoryData.h"
#include "setup/SetupManager.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/utils.h"
#include "json/json.h"

namespace circuit {

CFactoryData::CFactoryData(CCircuitAI *circuit)
		: factoryIdx(0)
{
	/*
	 * Prepare factory choices
	 */
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& factories = root["factories"];
	factoryBuilds.reserve(factories.size());

	std::map<STerrainMapMobileType::Id, float> percents;
	CTerrainData& terrainData = circuit->GetGameAttribute()->GetTerrainData();
	const std::vector<STerrainMapImmobileType>& immobileType = terrainData.areaData0.immobileType;
	const std::vector<STerrainMapMobileType>& mobileType = terrainData.areaData0.mobileType;
	for (const std::string& fac : factories.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(fac.c_str());
		if (cdef == nullptr) {
			continue;
		}
		STerrainMapImmobileType::Id itId = cdef->GetImmobileId();
		if ((itId < 0) || !immobileType[itId].typeUsable) {
			continue;
		}

		SFactory sfac;
		sfac.id = cdef->GetId();
		// TODO: Replace importance with proper terrain analysis (size, hardness, unit's power, speed)
		const Json::Value& importance = factories[fac]["importance"];
		if (importance == Json::Value::null) {
			sfac.startImp = sfac.switchImp = 1.0f;
		} else {
			sfac.startImp = importance.get((unsigned)0, 1.0f).asFloat();
			sfac.switchImp = importance.get((unsigned)1, 1.0f).asFloat();
		}
		STerrainMapMobileType::Id mtId = cdef->GetMobileId();
		if (mtId < 0) {
			factoryBuilds.push_back(sfac);
			float offset = (float)rand() / RAND_MAX * 50.0;
			percents[sfac.id] = offset + sfac.startImp * 60.0;
		} else if (mobileType[mtId].typeUsable) {
			factoryBuilds.push_back(sfac);
			float offset = (float)rand() / RAND_MAX * 40.0 - 20.0;
			percents[sfac.id] = offset + sfac.startImp * mobileType[mtId].areaLargest->percentOfMap;
		}
	}
	auto cmp = [circuit, &percents](const SFactory& a, const SFactory& b) {
		return percents[a.id] > percents[b.id];
	};
	std::sort(factoryBuilds.begin(), factoryBuilds.end(), cmp);

	// Don't start with air
	if (!factoryBuilds.empty() && (circuit->GetCircuitDef(factoryBuilds.front().id)->GetMobileId() < 0)) {
		for (SFactory& fac : factoryBuilds) {
			if (circuit->GetCircuitDef(fac.id)->GetMobileId() >= 0) {
				std::swap(factoryBuilds.front(), fac);
				break;
			}
		}
	}
}

CFactoryData::~CFactoryData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

CCircuitDef* CFactoryData::GetFactoryToBuild(CCircuitAI* circuit)
{
	for (unsigned i = 0; i < factoryBuilds.size(); ++i) {
		CCircuitDef* cdef = circuit->GetCircuitDef(factoryBuilds[factoryIdx].id);
		if (cdef->IsAvailable()) {
			return cdef;
		}
		AdvanceFactoryIdx();
	}
	return nullptr;
}

} // namespace circuit
