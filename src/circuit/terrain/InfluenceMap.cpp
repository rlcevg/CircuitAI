/*
 * InfluenceMap.cpp
 *
 *  Created on: Oct 20, 2019
 *      Author: rlcevg
 */

#include "terrain/InfluenceMap.h"
#include "terrain/TerrainManager.h"
#include "module/EconomyManager.h"
#include "unit/CircuitUnit.h"
#include "unit/EnemyUnit.h"
#include "util/utils.h"
// FIXME: DEBUG
#include "terrain/ThreatMap.h"
// FIXME: DEBUG

#include "OOAICallback.h"
#include "Cheats.h"
#include "Feature.h"
#include "FeatureDef.h"

namespace circuit {

using namespace springai;

CInfluenceMap::CInfluenceMap(CCircuitAI* circuit)
		: circuit(circuit)
		, inflCostMax(0.f)
		, vulnMaxInfl(0.f)
{
	squareSize = circuit->GetTerrainManager()->GetConvertStoP() * 4;
	width = circuit->GetTerrainManager()->GetSectorXSize() / 4;
	height = circuit->GetTerrainManager()->GetSectorZSize() / 4;
	mapSize = width * height;

	enemyInfl.resize(mapSize, INFL_BASE);
	allyInfl.resize(mapSize, INFL_BASE);
	influence.resize(mapSize, INFL_BASE);
	influenceCost.resize((width * 4 + 2) * (height * 4 + 2), THREAT_BASE);
	tension.resize(mapSize, INFL_BASE);
	vulnerability.resize(mapSize, INFL_BASE);
	featureInfl.resize(mapSize, INFL_BASE);
}

CInfluenceMap::~CInfluenceMap()
{
#ifdef DEBUG_VIS
	for (const std::pair<Uint32, float*>& win : sdlWindows) {
		circuit->GetDebugDrawer()->DelSDLWindow(win.first);
		delete[] win.second;
	}
#endif
}

void CInfluenceMap::Update()
{
	Clear();
	const CCircuitAI::EnemyUnits& hostiles = circuit->GetThreatMap()->GetHostileUnits();
	for (auto& kv : hostiles) {
		CEnemyUnit* e = kv.second;
		AddUnit(e);
	}
	const CCircuitAI::Units& units = circuit->GetTeamUnits();
	for (auto& kv : units) {
		CCircuitUnit* u = kv.second;
		if (u->GetCircuitDef()->IsAttacker()) {
			AddUnit(u);
		}
	}
	float maxInfl = 0.f;
	for (size_t i = 0; i < influence.size(); ++i) {
		influence[i] = allyInfl[i] - enemyInfl[i];
		int y = (i / width);
		int x = i - (y * width);
		for (int ii = 0; ii < 4; ++ii) {
			for (int jj = 0; jj < 4; ++jj) {
				size_t ind = (y * 4 + jj + 1) * (width * 4 + 2) + x * 4 + ii + 1;
				influenceCost[ind] = -influence[i];
			}
		}
		if (maxInfl < influence[i]) {
			maxInfl = influence[i];
		}
	}
	inflCostMax = 0.f;
	for (size_t i = 0; i < influenceCost.size(); ++i) {
		influenceCost[i] += maxInfl + THREAT_BASE;
		if (inflCostMax < influenceCost[i]) {
			inflCostMax = influenceCost[i];
		}
	}
	for (size_t i = 0; i < tension.size(); ++i) {
		tension[i] = allyInfl[i] + enemyInfl[i];
	}
	vulnMaxInfl = 0.f;
	for (size_t i = 0; i < vulnerability.size(); ++i) {
		vulnerability[i] = tension[i] - abs(influence[i]);
		if (vulnMaxInfl < vulnerability[i]) {
			vulnMaxInfl = vulnerability[i];
		}
	}
	Cheats* cheats = circuit->GetCallback()->GetCheats();
	cheats->SetEnabled(true);
	auto features = std::move(circuit->GetCallback()->GetFeatures());
	for (Feature* f : features) {
		if (f == nullptr) {
			continue;
		}
		AddFeature(f);
		delete f;
	}
	cheats->SetEnabled(false);
	delete cheats;

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CInfluenceMap::AddUnit(CCircuitUnit* u)
{
	int posx, posz;
	PosToXZ(u->GetPos(circuit->GetLastFrame()), posx, posz);

	const float val = u->GetCircuitDef()->GetPower();
	// FIXME: GetInfluenceRange: for statics it's just range; mobile should account for speed
	const int range = u->GetCircuitDef()->GetThreatRange(CCircuitDef::ThreatType::LAND);
	const int rangeSq = SQUARE(range);

	const int beginX = std::max(int(posx - range + 1),       0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),       0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int x = beginX; x < endX; ++x) {
		const int dxSq = SQUARE(posx - x);
		for (int z = beginZ; z < endZ; ++z) {
			const int dzSq = SQUARE(posz - z);
			const int lenSq = dxSq + dzSq;
			if (lenSq > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
			allyInfl[index] += infl;
		}
	}
}

//void CInfluenceMap::DelUnit(CCircuitUnit* u)
//{
//	int posx, posz;
//	PosToXZ(e->GetPos(), posx, posz);
//	const int widthSec = width - 2;
//
//	const float threat = e->GetThreat()/* + THREAT_DECAY*/;
//	const int rangeLand = e->GetRange(CCircuitDef::ThreatType::LAND);
//	const int rangeLandSq = SQUARE(rangeLand);
//	const int rangeWater = e->GetRange(CCircuitDef::ThreatType::WATER);
//	const int rangeWaterSq = SQUARE(rangeWater);
//	const int range = std::max(rangeLand, rangeWater);
//	const std::vector<STerrainMapSector>& sector = areaData->sector;
//
//	const int beginX = std::max(int(posx - range + 1),          1);
//	const int endX   = std::min(int(posx + range    ),  width - 1);
//	const int beginZ = std::max(int(posz - range + 1),          1);
//	const int endZ   = std::min(int(posz + range    ), height - 1);
//
//	for (int x = beginX; x < endX; ++x) {
//		const int dxSq = SQUARE(posx - x);
//		for (int z = beginZ; z < endZ; ++z) {
//			const int dzSq = SQUARE(posz - z);
//
//			const int sum = dxSq + dzSq;
//			const int index = z * width + x;
//			const int idxSec = (z - 1) * widthSec + (x - 1);
//			const float heat = threat * (1.5f - 1.0f * sqrtf(sum) / range);
//			bool isWaterThreat = (sum <= rangeWaterSq) && sector[idxSec].isWater;
//			if (isWaterThreat || ((sum <= rangeLandSq) && (sector[idxSec].position.y >= -SQUARE_SIZE * 5)))
//			{
//				amphThreat[index] = std::max<float>(amphThreat[index] - heat, THREAT_BASE);
//			}
//			if (isWaterThreat || (sum <= rangeLandSq)) {
//				surfThreat[index] = std::max<float>(surfThreat[index] - heat, THREAT_BASE);
//			}
//		}
//	}
//}

void CInfluenceMap::AddUnit(CEnemyUnit* e)
{
	int posx, posz;
	PosToXZ(e->GetPos(), posx, posz);

	const float val = e->GetThreat();
	// FIXME: GetInfluenceRange: for statics it's just range; mobile should account for speed
	const int range = e->GetRange(CCircuitDef::ThreatType::LAND);
	const int rangeSq = SQUARE(range);

	const int beginX = std::max(int(posx - range + 1),       0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),       0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int x = beginX; x < endX; ++x) {
		const int dxSq = SQUARE(posx - x);
		for (int z = beginZ; z < endZ; ++z) {
			const int dzSq = SQUARE(posz - z);
			const int lenSq = dxSq + dzSq;
			if (lenSq > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
			enemyInfl[index] += infl;
		}
	}
}

//void CInfluenceMap::DelUnit(CEnemyUnit* e)
//{
//	int posx, posz;
//	PosToXZ(e->GetPos(), posx, posz);
//
//	const float val = e->GetThreat();
//	const int range = e->GetRange(CCircuitDef::ThreatType::LAND);
//	const int rangeSq = SQUARE(range);
//
//	const int beginX = std::max(int(posx - range + 1),       0);
//	const int endX   = std::min(int(posx + range    ),  width);
//	const int beginZ = std::max(int(posz - range + 1),       0);
//	const int endZ   = std::min(int(posz + range    ), height);
//
//	for (int x = beginX; x < endX; ++x) {
//		const int dxSq = SQUARE(posx - x);
//		for (int z = beginZ; z < endZ; ++z) {
//			const int dzSq = SQUARE(posz - z);
//			const int lenSq = dxSq + dzSq;
//			if (lenSq > rangeSq) {
//				continue;
//			}
//
//			const int index = z * width + x;
//			const float infl = val * sqrtf(lenSq) / range;
//			enemyInfl[index] = std::max<float>(enemyInfl[index] - infl, INFL_BASE);
//		}
//	}
//}

void CInfluenceMap::AddFeature(Feature* f)
{
	int posx, posz;
	PosToXZ(f->GetPosition(), posx, posz);

	FeatureDef* featDef = f->GetDef();
	if (!featDef->IsReclaimable()) {
		delete featDef;
		return;
	}
	const float val = featDef->GetContainedResource(circuit->GetEconomyManager()->GetMetalRes()) * f->GetReclaimLeft();
	delete featDef;
	const int range = 4;
	const int rangeSq = SQUARE(range);

	const int beginX = std::max(int(posx - range + 1),       0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),       0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int x = beginX; x < endX; ++x) {
		const int dxSq = SQUARE(posx - x);
		for (int z = beginZ; z < endZ; ++z) {
			const int dzSq = SQUARE(posz - z);
			const int lenSq = dxSq + dzSq;
			if (lenSq > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
			featureInfl[index] += infl;
		}
	}
}

inline void CInfluenceMap::PosToXZ(const AIFloat3& pos, int& x, int& z) const
{
	x = (int)pos.x / squareSize;
	z = (int)pos.z / squareSize;
}

#ifdef DEBUG_VIS
#define ENEMY(x, i, v) {	\
	x[i * 3 + 0] = .9f * v;  /*R*/	\
	x[i * 3 + 1] = .4f * v;  /*G*/	\
	x[i * 3 + 2] = .1f * v;  /*B*/	\
}
#define ALLY(x, i, v) {	\
	x[i * 3 + 0] = .1f * v;  /*R*/	\
	x[i * 3 + 1] = .4f * v;  /*G*/	\
	x[i * 3 + 2] = .9f * v;  /*B*/	\
}

void CInfluenceMap::UpdateVis()
{
	if (sdlWindows.empty()) {
		return;
	}

	Uint32 sdlWindowId;
	float* dbgMap;
	std::tie(sdlWindowId, dbgMap) = sdlWindows[0];
	for (unsigned i = 0; i < enemyInfl.size(); ++i) {
		dbgMap[i] = std::min<float>((enemyInfl[i] - INFL_BASE) / 40.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {255, 100, 0, 0});

	std::tie(sdlWindowId, dbgMap) = sdlWindows[1];
	for (unsigned i = 0; i < allyInfl.size(); ++i) {
		dbgMap[i] = std::min<float>((allyInfl[i] - INFL_BASE) / 40.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {0, 100, 255, 0});

	std::tie(sdlWindowId, dbgMap) = sdlWindows[2];
	for (unsigned i = 0; i < influence.size(); ++i) {
		float value = utils::clamp(influence[i] / 40.0f, -1.f, 1.f);
		if (value < 0) ENEMY(dbgMap, i, -value)
		else ALLY(dbgMap, i, value)
	}
	circuit->GetDebugDrawer()->DrawTex(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[3];
	for (unsigned i = 0; i < tension.size(); ++i) {
		dbgMap[i] = std::min<float>(tension[i] / 40.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {255, 100, 0, 0});

	std::tie(sdlWindowId, dbgMap) = sdlWindows[4];
	for (unsigned i = 0; i < vulnerability.size(); ++i) {
		float value = utils::clamp((vulnerability[i] - vulnMaxInfl / 2) / (vulnMaxInfl / 2), -1.f, 1.f);
		if (value < 0) ALLY(dbgMap, i, -value)
		else ENEMY(dbgMap, i, value)
	}
	circuit->GetDebugDrawer()->DrawTex(sdlWindowId, dbgMap);

	std::tie(sdlWindowId, dbgMap) = sdlWindows[5];
	for (unsigned i = 0; i < featureInfl.size(); ++i) {
		dbgMap[i] = std::min<float>((featureInfl[i] - INFL_BASE) / 500.f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {0, 100, 255, 0});

	std::tie(sdlWindowId, dbgMap) = sdlWindows[6];
	for (unsigned i = 0; i < influenceCost.size(); ++i) {
		dbgMap[i] = std::min<float>((influenceCost[i] - THREAT_BASE) / inflCostMax, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {220, 200, 50, 0});
}

void CInfluenceMap::ToggleVis()
{
	if (sdlWindows.empty()) {
		// ~infl
		std::pair<Uint32, float*> win;
		std::string label;

		win.second = new float [enemyInfl.size()];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Enemy Influence Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [allyInfl.size()];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Ally Influence Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [influence.size() * 3];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Influence Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [tension.size() * 3];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Tension Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [vulnerability.size() * 3];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Vulnerability Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [featureInfl.size()];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Feature Influence Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [influenceCost.size()];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Influence Cost Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width * 4 + 2, height * 4 + 2, label.c_str());
		sdlWindows.push_back(win);

		UpdateVis();
	} else {
		for (const std::pair<Uint32, float*>& win : sdlWindows) {
			circuit->GetDebugDrawer()->DelSDLWindow(win.first);
			delete[] win.second;
		}
		sdlWindows.clear();
	}
}
#endif

} // namespace circuit
