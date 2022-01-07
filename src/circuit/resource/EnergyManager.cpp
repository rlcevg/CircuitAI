/*
 * EnergyManager.cpp
 *
 *  Created on: Jun 25, 2021
 *      Author: rlcevg
 */

#include "resource/EnergyManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "Feature.h"
#include "FeatureDef.h"

namespace circuit {

using namespace springai;

CEnergyManager::CEnergyManager(CCircuitAI* circuit, CEnergyData* energyData)
		: circuit(circuit)
		, energyData(energyData)
{
	if (!energyData->IsInitialized()) {
		ParseGeoSpots();
	}
}

CEnergyManager::~CEnergyManager()
{
}

void CEnergyManager::ParseGeoSpots()
{
	std::vector<AIFloat3> spots;

	CCircuitDef* geoDef = circuit->GetEconomyManager()->GetSideInfo().geoDef;
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const unsigned width = circuit->GetMap()->GetWidth();
	const unsigned height = circuit->GetMap()->GetHeight();
	const int xsize = geoDef->GetDef()->GetXSize();
	const int zsize = geoDef->GetDef()->GetZSize();
	std::vector<Feature*> features = std::move(circuit->GetCallback()->GetFeatures());
	for (Feature* feature : features) {
		FeatureDef* featDef = feature->GetDef();
		const bool isGeo = featDef->IsGeoThermal();
		delete featDef;
		if (!isGeo) {
			continue;
		}
		// TODO: if( !TM->waterIsHarmful || cb->GetElevation(position.x,position.z) >= 0 ) ?
		AIFloat3 pos = feature->GetPosition();
		CTerrainManager::SnapPosition(pos);
		const unsigned x1 = int(pos.x) / SQUARE_SIZE - (xsize / 2), x2 = x1 + xsize;
		const unsigned z1 = int(pos.z) / SQUARE_SIZE - (zsize / 2), z2 = z1 + zsize;
		if ((x1 < x2) && (x2 < width) && (z1 < z2) && (z2 < height) &&
			terrainMgr->CanBeBuiltAt(geoDef, pos))
		{
			spots.push_back(pos);
		}
	}
	utils::free_clear(features);

	energyData->Init(std::move(spots));
}

bool CEnergyManager::IsSpotValid(int index, const AIFloat3& pos) const
{
	if ((index < 0) || ((size_t)index >= GetSpots().size())) {
		return false;
	}
	return utils::is_equal_pos(GetSpots()[index], pos);
}

} // namespace circuit
