/*
 * SpringMap.h
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SPRING_SPRINGMAP_H_
#define SRC_CIRCUIT_SPRING_SPRINGMAP_H_

#include "util/Defines.h"

#include "Map.h"

struct SSkirmishAICallback;

namespace circuit {

class CMap {
public:
	CMap(const struct SSkirmishAICallback* clb, springai::Map* m);
	virtual ~CMap();

	void GetHeightMap(FloatVec& outHeightMap) const;
	void GetSlopeMap(FloatVec& outSlopeMap) const;

	void GetRadarMap(IntVec& radarMap) const;
	void GetSonarMap(IntVec& sonarMap) const;
	void GetLosMap(IntVec& losMap) const;

	float GetElevationAt(float x, float z) const { return map->GetElevationAt(x, z); }
	int GetWidth() const { return map->GetWidth(); }
	int GetHeight() const { return map->GetHeight(); }
	const char* GetName() const { return map->GetName(); }

	bool IsPossibleToBuildAt(springai::UnitDef* unitDef, const springai::AIFloat3& pos, int facing) const {
		return map->IsPossibleToBuildAt(unitDef, pos, facing);
	}
	springai::AIFloat3 FindClosestBuildSite(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int minDist, int facing) {
		return map->FindClosestBuildSite(unitDef, pos, searchRadius, minDist, facing);
	}

	float GetWaterDamage() const { return map->GetWaterDamage(); }
	float GetMinWind() const { return map->GetMinWind(); }
	float GetMaxWind() const { return map->GetMaxWind(); }
	float GetCurWind() const { return map->GetCurWind(); }
	float GetTidalStrength() const { return map->GetTidalStrength(); }

	float GetExtractorRadius(springai::Resource* resource) { return map->GetExtractorRadius(resource); }
	void GetResourceMapSpotsPositions(springai::Resource* resource, F3Vec& spots) const;
	void GetResourceMap(springai::Resource* resource, ShortVec& metalMap) const;
	float GetMaxResource(springai::Resource* resource) { return map->GetMaxResource(resource); }

	springai::AIFloat3 GetMousePos() const { return map->GetMousePos(); }
	springai::Drawer* GetDrawer() const { return map->GetDrawer(); }

private:
	const struct SSkirmishAICallback* sAICallback;
	springai::Map* map;  // owner
};

} // namespace circuit

#endif // SRC_CIRCUIT_SPRING_SPRINGMAP_H_
