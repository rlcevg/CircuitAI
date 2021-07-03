/*
 * SpringMap.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#include "spring/SpringMap.h"

#include "SSkirmishAICallback.h"	// "direct" C API
#include "Resource.h"

namespace circuit {

using namespace springai;

CMap::CMap(const struct SSkirmishAICallback* clb, Map* m)
		: sAICallback(clb)
		, map(m)
{
}

CMap::~CMap()
{
	delete map;
}

void CMap::GetHeightMap(FloatVec& heightMap) const
{
	// NOTE: GetNextAreaData()->heightMap = std::move(map->GetHeightMap());
	if (heightMap.empty()) {
		heightMap.resize(sAICallback->Map_getHeightMap(map->GetSkirmishAIId(), nullptr, -1));
	}
	sAICallback->Map_getHeightMap(map->GetSkirmishAIId(), heightMap.data(), heightMap.size());
}

void CMap::GetSlopeMap(FloatVec& slopeMap) const
{
	// NOTE: slopeMap = std::move(map->GetSlopeMap());
	if (slopeMap.empty()) {
		slopeMap.resize(sAICallback->Map_getSlopeMap(map->GetSkirmishAIId(), nullptr, -1));
	}
	sAICallback->Map_getSlopeMap(map->GetSkirmishAIId(), slopeMap.data(), slopeMap.size());
}

void CMap::GetSonarMap(IntVec& sonarMap) const
{
	// NOTE: sonarMap = std::move(circuit->GetMap()->GetSonarMap());
	if (sonarMap.empty()) {
		sonarMap.resize(sAICallback->Map_getSonarMap(map->GetSkirmishAIId(), nullptr, -1));
	}
	sAICallback->Map_getSonarMap(map->GetSkirmishAIId(), sonarMap.data(), sonarMap.size());
}

void CMap::GetLosMap(IntVec& losMap) const
{
	// NOTE: losMap = std::move(circuit->GetMap()->GetLosMap());
	if (losMap.empty()) {
		losMap.resize(sAICallback->Map_getLosMap(map->GetSkirmishAIId(), nullptr, -1));
	}
	sAICallback->Map_getLosMap(map->GetSkirmishAIId(), losMap.data(), losMap.size());
}

void CMap::GetResourceMapSpotsPositions(Resource* resource, F3Vec& spots) const
{
	// NOTE: return map->GetResourceMapSpotsPositions(resource);
	int resourceId = resource->GetResourceId();
	int spots_size_raw = sAICallback->Map_getResourceMapSpotsPositions(map->GetSkirmishAIId(), resourceId, nullptr, -1);
	if (spots_size_raw % 3 != 0) {
		return;
	}
	int size = spots_size_raw / 3;
	float* spots_AposF3 = new float[spots_size_raw];
	sAICallback->Map_getResourceMapSpotsPositions(map->GetSkirmishAIId(), resourceId, spots_AposF3, spots_size_raw);
	spots.clear();
	spots.reserve(size);
	for (int i = 0; i < spots_size_raw; i += 3) {
		spots.push_back(AIFloat3(spots_AposF3[i], spots_AposF3[i + 1], spots_AposF3[i + 2]));
	}
	delete[] spots_AposF3;
}

} // namespace circuit
