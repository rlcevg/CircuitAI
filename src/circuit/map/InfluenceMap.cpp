/*
 * InfluenceMap.cpp
 *
 *  Created on: Oct 20, 2019
 *      Author: rlcevg
 */

#include "map/InfluenceMap.h"
#include "map/MapManager.h"
#include "module/EconomyManager.h"
#include "scheduler/Scheduler.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "unit/ally/AllyUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"
#include "util/Profiler.h"
#include "json/json.h"

#include "spring/SpringCallback.h"

//#include "Cheats.h"
#include "Feature.h"
#include "FeatureDef.h"

namespace circuit {

using namespace springai;

CInfluenceMap::CInfluenceMap(CMapManager* manager)
		: manager(manager)
//		, vulnMax(0.f)
		, isUpdating(false)
{
	CCircuitAI* circuit = manager->GetCircuit();
	squareSize = circuit->GetTerrainManager()->GetConvertStoP() * 4;
	width = circuit->GetTerrainManager()->GetSectorXSize() / 4;
	height = circuit->GetTerrainManager()->GetSectorZSize() / 4;
	mapSize = width * height;

	for (SInfluenceData* inflData : {&inflData0, &inflData1}) {
		inflData->enemyInfl.resize(mapSize, INFL_BASE);
		inflData->allyInfl.resize(mapSize, INFL_BASE);
		inflData->allyDefendInfl.resize(mapSize, INFL_BASE);
		inflData->influence.resize(mapSize, INFL_BASE);
//		inflData->tension.resize(mapSize, INFL_BASE);
//		inflData->vulnerability.resize(mapSize, INFL_BASE);
//		inflData->featureInfl.resize(mapSize, INFL_BASE);
	}

	enemyInfl = inflData0.enemyInfl.data();
	allyInfl = inflData0.allyInfl.data();
	allyDefendInfl = inflData0.allyDefendInfl.data();
	influence = inflData0.influence.data();
//	tension = inflData0.tension.data();
//	vulnerability = inflData0.vulnerability.data();
//	featureInfl = inflData0.featureInfl.data();

	drawEnemyInfl = inflData1.enemyInfl.data();
	drawAllyInfl = inflData1.allyInfl.data();
	drawAllyDefendInfl = inflData1.allyDefendInfl.data();
	drawInfluence = inflData1.influence.data();
//	drawTension = inflData1.tension.data();
//	drawVulnerability = inflData1.vulnerability.data();
//	drawFeatureInfl = inflData1.featureInfl.data();

	ReadConfig();
}

CInfluenceMap::~CInfluenceMap()
{
#ifdef DEBUG_VIS
	CCircuitAI* circuit = manager->GetCircuit();
	for (const std::pair<Uint32, float*>& win : sdlWindows) {
		circuit->GetDebugDrawer()->DelSDLWindow(win.first);
		delete[] win.second;
	}
#endif
}

void CInfluenceMap::ReadConfig()
{
	CCircuitAI* circuit = manager->GetCircuit();
	const Json::Value& defence = circuit->GetSetupManager()->GetConfig()["defence"];
	defRadius = defence.get("infl_rad", 5.f).asFloat();
}

void CInfluenceMap::EnqueueUpdate()
{
//	if (isUpdating) {  // NOTE: done at MapManager level
//		return;
//	}
	isUpdating = true;

	FrameMarkStart(profiler.GetInflUpdateName(manager->GetCircuit()->GetSkirmishAIId()));

	CCircuitAI* circuit = manager->GetCircuit();
	CEnemyManager* enemyMgr = circuit->GetEnemyManager();
	circuit->GetScheduler()->RunPriorityJob(CScheduler::WorkJob(&CInfluenceMap::Update, this, enemyMgr));
}

void CInfluenceMap::Prepare(SInfluenceData& inflData)
{
	std::fill(inflData.enemyInfl.begin(), inflData.enemyInfl.end(), INFL_BASE);
	std::fill(inflData.allyInfl.begin(), inflData.allyInfl.end(), INFL_BASE);
	std::fill(inflData.allyDefendInfl.begin(), inflData.allyDefendInfl.end(), INFL_BASE);
	std::fill(inflData.influence.begin(), inflData.influence.end(), INFL_BASE);
//	std::fill(inflData.tension.begin(), inflData.tension.end(), INFL_BASE);
//	std::fill(inflData.vulnerability.begin(), inflData.vulnerability.end(), INFL_BASE);
//	std::fill(inflData.featureInfl.begin(), inflData.featureInfl.end(), INFL_BASE);

	drawEnemyInfl = inflData.enemyInfl.data();
	drawAllyInfl = inflData.allyInfl.data();
	drawAllyDefendInfl = inflData.allyDefendInfl.data();
	drawInfluence = inflData.influence.data();
//	drawTension = inflData.tension.data();
//	drawVulnerability = inflData.vulnerability.data();
//	drawFeatureInfl = inflData.featureInfl.data();
}

std::shared_ptr<IMainJob> CInfluenceMap::Update(CEnemyManager* enemyMgr)
{
	ZoneScopedN(__PRETTY_FUNCTION__);

	Prepare(*GetNextInflData());

	for (const SEnemyData& e : enemyMgr->GetHostileDatas()) {
		AddEnemy(e);
	}

	return CScheduler::GameJob(&CInfluenceMap::Apply, this);
}

void CInfluenceMap::Apply()
{
	CCircuitAI* circuit = manager->GetCircuit();
	circuit->UpdateFriendlyUnits();  // FIXME: update or ignore units with -RgtVector position
	const CAllyTeam::AllyUnits& units = circuit->GetFriendlyUnits();
	for (auto& kv : units) {
		CAllyUnit* u = kv.second;
		if (u->GetCircuitDef()->IsAttacker()) {
			if (u->GetCircuitDef()->IsMobile()) {
				AddMobileArmed(u);
			} else {
				AddStaticArmed(u);
			}
		} else {
			AddUnarmed(u);
		}
	}
	for (int i = 0; i < mapSize; ++i) {
		drawInfluence[i] = drawAllyInfl[i] - drawEnemyInfl[i];
	}
//	for (int i = 0; i < mapSize; ++i) {
//		drawTension[i] = drawAllyInfl[i] + drawEnemyInfl[i];
//	}
//	vulnMax = 0.f;
//	for (int i = 0; i < mapSize; ++i) {
//		drawVulnerability[i] = drawTension[i] - std::fabs(drawInfluence[i]);
//		if (vulnMax < drawVulnerability[i]) {
//			vulnMax = drawVulnerability[i];
//		}
//	}
//	Cheats* cheats = circuit->GetCheats();
//	cheats->SetEnabled(true);
//	auto features = circuit->GetCallback()->GetFeatures();
//	for (Feature* f : features) {
//		if (f == nullptr) {
//			continue;
//		}
//		AddFeature(f);
//		delete f;
//	}
//	cheats->SetEnabled(false);

	SwapBuffers();
	isUpdating = false;

	FrameMarkEnd(profiler.GetInflUpdateName(manager->GetCircuit()->GetSkirmishAIId()));

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CInfluenceMap::SwapBuffers()
{
	pInflData = GetNextInflData();
	SInfluenceData& inflData = *pInflData.load();
	enemyInfl = inflData.enemyInfl.data();
	allyInfl = inflData.allyInfl.data();
	allyDefendInfl = inflData.allyDefendInfl.data();
	influence = inflData.influence.data();
//	tension = inflData.tension.data();
//	vulnerability = inflData.vulnerability.data();
//	featureInfl = inflData.featureInfl.data();
}

float CInfluenceMap::GetEnemyInflAt(const AIFloat3& position) const
{
	int x, z;
	PosToXZ(position, x, z);
	return enemyInfl[z * width + x] - INFL_BASE;
}

float CInfluenceMap::GetAllyInflAt(const springai::AIFloat3& position) const
{
	int x, z;
	PosToXZ(position, x, z);
	return allyInfl[z * width + x] - INFL_BASE;
}

float CInfluenceMap::GetAllyDefendInflAt(const AIFloat3& position) const
{
	int x, z;
	PosToXZ(position, x, z);
	return allyDefendInfl[z * width + x] - INFL_BASE;
}

float CInfluenceMap::GetInfluenceAt(const AIFloat3& position) const
{
	int x, z;
	PosToXZ(position, x, z);
	return influence[z * width + x] - INFL_BASE;
}

int CInfluenceMap::Pos2Index(const AIFloat3& pos) const
{
	return int(pos.z / squareSize) * width + int(pos.x / squareSize);
}

int CInfluenceMap::GetUnitRange(CAllyUnit* u) const
{
	const CCircuitDef* cdef = u->GetCircuitDef();

	// FIXME: DEBUG  comm's threat value is not based on proper weapons
//	if (cdef->IsRoleComm()) {
//		CCircuitAI* circuit = manager->GetCircuit();
//		// TODO: by weapons 1,2 descriptions set proper land/air/water ranges/threats
//		float maxRange = 0.f;
//		float maxAoe = 0.f;
//		for (int num = 1; num < 3; ++num) {
//			std::string str = utils::int_to_string(num, "comm_weapon_id_%i");
//			int weaponDefId = int(u->GetUnit()->GetRulesParamFloat(str.c_str(), -1));
//			if (weaponDefId < 0) {
//				continue;
//			}
//
//			CWeaponDef* weaponDef = circuit->GetWeaponDef(weaponDefId);
//			const float range = weaponDef->GetRange();
//			if (maxRange < range) {
//				maxRange = range;
//				maxAoe = weaponDef->GetAoe();
//			}
//		}
//		const float mult = u->GetUnit()->GetRulesParamFloat("comm_range_mult", 1.f);
//		return int(maxRange * mult + maxAoe / 2) / squareSize * 4 + 1;
//	} else {
	// FIXME: DEBUG

		if (cdef->GetMaxRange() > 1000.f) {
			return cdef->GetThreatRange(CCircuitDef::ThreatType::SURF) / 2;
		}
		return cdef->GetThreatRange(CCircuitDef::ThreatType::SURF);
//	}
}

void CInfluenceMap::AddMobileArmed(CAllyUnit* u)
{
	CCircuitAI* circuit = manager->GetCircuit();  // FIXME: not thread-safe
	int posx, posz;
	PosToXZ(u->GetPos(circuit->GetLastFrame()), posx, posz);

	const float val = u->GetCircuitDef()->GetPower();
	// FIXME: GetInfluenceRange: for statics it's just range; mobile should account for speed
	const int range = GetUnitRange(u);
	const int rangeSq = SQUARE(range);

	const int beginX = std::max(int(posx - range + 1),       0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),       0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);
			const int lenSq = dxSq + dzSq;
			if (lenSq > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
			drawAllyInfl[index] += infl;
		}
	}
}

void CInfluenceMap::AddStaticArmed(CAllyUnit* u)
{
	CCircuitAI* circuit = manager->GetCircuit();  // FIXME: not thread-safe
	int posx, posz;
	PosToXZ(u->GetPos(circuit->GetLastFrame()), posx, posz);

	const float val = u->GetCircuitDef()->GetPower();
	const int range = u->GetCircuitDef()->GetThreatRange(CCircuitDef::ThreatType::SURF) / 2;
	const int rangeSq = SQUARE(range);

	const int beginX = std::max(int(posx - range + 1),       0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),       0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);
			const int lenSq = dxSq + dzSq;
			if (lenSq > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
			drawAllyInfl[index] += infl;
			drawAllyDefendInfl[index] += infl;
		}
	}
}

void CInfluenceMap::AddUnarmed(CAllyUnit* u)
{
	CCircuitAI* circuit = manager->GetCircuit();  // FIXME: not thread-safe
	int posx, posz;
	PosToXZ(u->GetPos(circuit->GetLastFrame()), posx, posz);

	const float val = 2.f;
	const int range = DEFAULT_SLACK * 4 * defRadius / squareSize;
	const int rangeSq = SQUARE(range);

	const int beginX = std::max(int(posx - range + 1),       0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),       0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);
			const int lenSq = dxSq + dzSq;
			if (lenSq > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
			drawAllyDefendInfl[index] += infl;
		}
	}
}

void CInfluenceMap::AddEnemy(const SEnemyData& e)
{
	int posx, posz;

	PosToXZ(e.pos, posx, posz);

	const float val = e.influence;
	// FIXME: GetInfluenceRange: for statics it's just range; mobile should account for speed
	const int range = (e.cdef == nullptr)
			? e.GetRange(CCircuitDef::ThreatType::SURF)
			: e.cdef->IsMobile()
					? e.GetRange(CCircuitDef::ThreatType::SURF)
					: e.GetRange(CCircuitDef::ThreatType::SURF) / 2;
	const int rangeSq = SQUARE(range);

	const int beginX = std::max(int(posx - range + 1),       0);
	const int endX   = std::min(int(posx + range    ),  width);
	const int beginZ = std::max(int(posz - range + 1),       0);
	const int endZ   = std::min(int(posz + range    ), height);

	for (int z = beginZ; z < endZ; ++z) {
		const int dzSq = SQUARE(posz - z);
		for (int x = beginX; x < endX; ++x) {
			const int dxSq = SQUARE(posx - x);
			const int lenSq = dxSq + dzSq;
			if (lenSq > rangeSq) {
				continue;
			}

			const int index = z * width + x;
			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
			drawEnemyInfl[index] += infl;
		}
	}
}

//void CInfluenceMap::AddFeature(Feature* f)
//{
//	CCircuitAI* circuit = manager->GetCircuit();
//	int posx, posz;
//	PosToXZ(f->GetPosition(), posx, posz);
//
//	FeatureDef* featDef = f->GetDef();
//	if (!featDef->IsReclaimable()) {
//		delete featDef;
//		return;
//	}
//	const float val = featDef->GetContainedResource(circuit->GetEconomyManager()->GetMetalRes()) * f->GetReclaimLeft();
//	delete featDef;
//	const int range = 2;
//	const int rangeSq = SQUARE(range);
//
//	const int beginX = std::max(int(posx - range + 1),       0);
//	const int endX   = std::min(int(posx + range    ),  width);
//	const int beginZ = std::max(int(posz - range + 1),       0);
//	const int endZ   = std::min(int(posz + range    ), height);
//
//	for (int z = beginZ; z < endZ; ++z) {
//		const int dzSq = SQUARE(posz - z);
//		for (int x = beginX; x < endX; ++x) {
//			const int dxSq = SQUARE(posx - x);
//			const int lenSq = dxSq + dzSq;
//			if (lenSq > rangeSq) {
//				continue;
//			}
//
//			const int index = z * width + x;
//			const float infl = val * (1.0f - 1.0f * sqrtf(lenSq) / range);
//			drawFeatureInfl[index] += infl;
//		}
//	}
//}

inline void CInfluenceMap::PosToXZ(const AIFloat3& pos, int& x, int& z) const
{
	x = (int)pos.x / squareSize;
	z = (int)pos.z / squareSize;
}

#ifdef DEBUG_VIS
#define ENEMY(x, i, v) {	\
	x[i * 3 + 0] = .10f * v;  /*R*/	\
	x[i * 3 + 1] = .40f * v;  /*G*/	\
	x[i * 3 + 2] = .95f * v;  /*B*/	\
}
#define ALLY(x, i, v) {	\
	x[i * 3 + 0] = .95f * v;  /*R*/	\
	x[i * 3 + 1] = .40f * v;  /*G*/	\
	x[i * 3 + 2] = .10f * v;  /*B*/	\
}

void CInfluenceMap::UpdateVis()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (isWidgetDrawing || isWidgetPrinting) {
		std::ostringstream cmd;
		cmd << "ai_thr_data:";
		cmd.write(reinterpret_cast<const char*>(influence), mapSize * sizeof(float));
		std::string s = cmd.str();
		circuit->GetLua()->CallRules(s.c_str(), s.size());
	}

	if (sdlWindows.empty()) {
		return;
	}

	Uint32 sdlWindowId;
	float* dbgMap;
	std::tie(sdlWindowId, dbgMap) = sdlWindows[0];
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((enemyInfl[i] - INFL_BASE) / 200.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {10, 50, 255, 0});

	std::tie(sdlWindowId, dbgMap) = sdlWindows[1];
	for (int i = 0; i < mapSize; ++i) {
		dbgMap[i] = std::min<float>((allyInfl[i] - INFL_BASE) / 200.0f, 1.0f);
	}
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {255, 50, 10, 0});

	std::tie(sdlWindowId, dbgMap) = sdlWindows[2];
	for (int i = 0; i < mapSize; ++i) {
		float value = utils::clamp(influence[i] / 200.0f, -1.f, 1.f);
		if (value < 0) ENEMY(dbgMap, i, -value)
		else ALLY(dbgMap, i, value)
	}
	circuit->GetDebugDrawer()->DrawTex(sdlWindowId, dbgMap);

//	std::tie(sdlWindowId, dbgMap) = sdlWindows[3];
//	for (int i = 0; i < mapSize; ++i) {
//		dbgMap[i] = std::min<float>(tension[i] / 200.0f, 1.0f);
//	}
//	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {255, 50, 10, 0});
//
//	std::tie(sdlWindowId, dbgMap) = sdlWindows[4];
//	for (int i = 0; i < mapSize; ++i) {
//		float value = utils::clamp((vulnerability[i] - vulnMax / 2) / (vulnMax / 2), -1.f, 1.f);
//		if (value < 0) ALLY(dbgMap, i, -value)
//		else ENEMY(dbgMap, i, value)
//	}
//	circuit->GetDebugDrawer()->DrawTex(sdlWindowId, dbgMap);
//
//	std::tie(sdlWindowId, dbgMap) = sdlWindows[5];
//	for (int i = 0; i < mapSize; ++i) {
//		dbgMap[i] = std::min<float>((featureInfl[i] - INFL_BASE) / 500.f, 1.0f);
//	}
//	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {10, 50, 255, 0});
}

void CInfluenceMap::ToggleSDLVis()
{
	CCircuitAI* circuit = manager->GetCircuit();
	if (sdlWindows.empty()) {
		// ~infl
		std::pair<Uint32, float*> win;
		std::string label;

		win.second = new float [mapSize];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Enemy Influence Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [mapSize];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Ally Influence Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

		win.second = new float [mapSize * 3];
		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Influence Map");
		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
		sdlWindows.push_back(win);

//		win.second = new float [mapSize * 3];
//		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Tension Map");
//		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
//		sdlWindows.push_back(win);
//
//		win.second = new float [mapSize * 3];
//		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Vulnerability Map");
//		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
//		sdlWindows.push_back(win);
//
//		win.second = new float [mapSize];
//		label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Feature Influence Map");
//		win.first = circuit->GetDebugDrawer()->AddSDLWindow(width, height, label.c_str());
//		sdlWindows.push_back(win);

		UpdateVis();
	} else {
		for (const std::pair<Uint32, float*>& win : sdlWindows) {
			circuit->GetDebugDrawer()->DelSDLWindow(win.first);
			delete[] win.second;
		}
		sdlWindows.clear();
	}
}

void CInfluenceMap::ToggleWidgetDraw()
{
	CCircuitAI* circuit = manager->GetCircuit();
	std::string cmd("ai_thr_draw:");
	std::string result = circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

	isWidgetDrawing = (result == "1");
	if (isWidgetDrawing) {
		cmd = utils::int_to_string(squareSize, "ai_thr_size:%i");
		cmd += utils::float_to_string(INFL_BASE, " %f");
		circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

		UpdateVis();
	}
}

void CInfluenceMap::ToggleWidgetPrint()
{
	CCircuitAI* circuit = manager->GetCircuit();
	std::string cmd("ai_thr_print:");
	std::string result = circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

	isWidgetPrinting = (result == "1");
	if (isWidgetPrinting) {
		cmd = utils::int_to_string(squareSize, "ai_thr_size:%i");
		cmd += utils::float_to_string(INFL_BASE, " %f");
		circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

		UpdateVis();
	}
}

void CInfluenceMap::SetMaxThreat(float maxThreat)
{
	CCircuitAI* circuit = manager->GetCircuit();
	std::string cmd = utils::float_to_string(maxThreat, "ai_thr_div:%f");
	circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());
}
#endif

} // namespace circuit
