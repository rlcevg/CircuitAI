/*
 * TerrainData.cpp
 *
 *  Created on: Dec 15, 2014
 *      Author: rlcevg
 */

#include "terrain/TerrainData.h"
#include "terrain/TerrainManager.h"
#include "scheduler/Scheduler.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/math/HierarchCluster.h"
#include "util/math/RagMatrix.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Log.h"
#include "MoveData.h"
//#include "File.h"

#include <functional>
#include <algorithm>
#include <deque>
#include <set>
#include <sstream>
#include <BWTA.h>  // FIXME: DEBUG
#include <bwta2/Timer.h>

namespace circuit {

using namespace springai;

#define AREA_UPDATE_RATE	(FRAMES_PER_SEC * 10)
// FIXME: Make Engine consts available to AI. @see rts/Sim/MoveTypes/MoveDefHandler.cpp
#define MAX_ALLOWED_WATER_DAMAGE_GMM	1e3f
#define MAX_ALLOWED_WATER_DAMAGE_HMM	1e4f

float CTerrainData::boundX(0.f);
float CTerrainData::boundZ(0.f);
int CTerrainData::convertStoP(1);
CMap* CTerrainData::map(nullptr);
int drawMTID = 0;  // FIXME: DEBUG
// >> BWTA2
CTerrainData* g_TerrainData;
CCircuitAI* g_Circuit;
// BWTA2 <<

CTerrainData::CTerrainData()
		: pAreaData(&areaData0)
		, waterIsHarmful(false)
		, waterIsAVoid(false)
		, sectorXSize(0)
		, sectorZSize(0)
, m_maxAltitude(1.f)
		, gameAttribute(nullptr)
		, isUpdating(false)
		, aiToUpdate(0)
		, isInitialized(false)
#ifdef DEBUG_VIS
		, toggleFrame(-1)
#endif
{
}

CTerrainData::~CTerrainData()
{
#ifdef DEBUG_VIS
	if (debugDrawer != nullptr) {
		for (const std::pair<Uint32, float*>& win : sdlWindows) {
			debugDrawer->DelSDLWindow(win.first);
			delete[] win.second;
		}
		debugDrawer = nullptr;
	}
#endif
}

void CTerrainData::Init(CCircuitAI* circuit)
{
	map = circuit->GetMap();
	scheduler = circuit->GetScheduler();
	gameAttribute = circuit->GetGameAttribute();
	circuit->LOG("Loading the Terrain-Map ...");

	/*
	 *  Assign areaData references
	 */
	SAreaData& areaData = *pAreaData.load();
	std::vector<STerrainMapMobileType>& mobileType = areaData.mobileType;
	std::vector<STerrainMapImmobileType>& immobileType = areaData.immobileType;
	std::vector<STerrainMapAreaSector>& sectorAirType = areaData.sectorAirType;
	std::vector<STerrainMapSector>& sector = areaData.sector;
	float& minElevation = areaData.minElevation;
	float& maxElevation = areaData.maxElevation;
	float& percentLand = areaData.percentLand;

	/*
	 *  Reading the WaterDamage and establishing sector size
	 */
	waterIsHarmful = false;
	waterIsAVoid = false;

	float waterDamage = map->GetWaterDamage();  // scaled by (UNIT_SLOWUPDATE_RATE / GAME_SPEED)
	std::string waterText = "  Water Damage: " + utils::float_to_string(waterDamage/*, "%-.*G"*/);
	// @see rts/Sim/MoveTypes/MoveDefHandler.cpp
	if (waterDamage > 0) {  // >= MAX_ALLOWED_WATER_DAMAGE_GMM
		waterIsHarmful = true;
		waterText += " (This map's water is harmful to land units";
//		if (waterDamage >= MAX_ALLOWED_WATER_DAMAGE_HMM) {  // TODO: Mark water blocks as threat?
			waterIsAVoid = true;
			waterText += " as well as hovercraft";
//		}
		waterText += ")";
	}
	circuit->LOG(waterText.c_str());

//	Map* map = circuit->GetMap();
//	std::string mapArchiveFileName = "maps/";
//	mapArchiveFileName += utils::MakeFileSystemCompatible(map->GetName());
//	mapArchiveFileName += ".smd";
//
//	File* file = circuit->GetCallback()->GetFile();
//	int mapArchiveFileSize = file->GetSize(mapArchiveFileName.c_str());
//	if (mapArchiveFileSize > 0) {
//		circuit->LOG("Searching the Map-Archive File: '%s'  File Size: %i", mapArchiveFileName.c_str(), mapArchiveFileSize);
//		char* archiveFile = new char[mapArchiveFileSize];
//		file->GetContent(mapArchiveFileName.c_str(), archiveFile, mapArchiveFileSize);
//		int waterDamage = GetFileValue(mapArchiveFileSize, archiveFile, "WaterDamage");
//		waterIsAVoid = GetFileValue(mapArchiveFileSize, archiveFile, "VoidWater") > 0;
//		circuit->LOG("  Void Water: %s", waterIsAVoid ? "true  (This map has no water)" : "false");
//
//		std::string waterText = "  Water Damage: " + utils::int_to_string(waterDamage);
//		if (waterDamage > 0) {
//			waterIsHarmful = true;
//			waterText += " (This map's water is harmful to land units";
//			if (waterDamage > 10000) {
//				waterIsAVoid = true; // UNTESTED
//				waterText += " as well as hovercraft";
//			}
//			waterText += ")";
//		}
//		circuit->LOG(waterText.c_str());
//		delete [] archiveFile;
//	} else {
//		circuit->LOG("Could not find Map-Archive file for reading additional map info: %s", mapArchiveFileName.c_str());
//	}
//	delete file;

	int mapWidth = map->GetWidth();
	int mapHeight = map->GetHeight();
	AIFloat3::maxxpos = mapWidth * SQUARE_SIZE;
	AIFloat3::maxzpos = mapHeight * SQUARE_SIZE;
	boundX = AIFloat3::maxxpos + BOUND_EXT;
	boundZ = AIFloat3::maxzpos + BOUND_EXT;
	areaData.heightMapSizeX = mapWidth;
	convertStoP = DEFAULT_SLACK;  // = 2^x, should not be less than 16 (2*SUQARE_SIZE)
	constexpr int SMALL_MAP = 8;
	constexpr int LARGE_MAP = 16;
	if ((mapWidth / 64) * (mapHeight / 64) < SMALL_MAP * SMALL_MAP) {
		convertStoP /= 2; // Smaller Sectors, more detailed analysis
	} else if ((mapWidth / 64) * (mapHeight / 64) > LARGE_MAP * LARGE_MAP) {
		convertStoP *= 2; // Larger Sectors, less detailed analysis
	}
	convertStoP = 16;  // FIXME: DEBUG
	sectorXSize = (SQUARE_SIZE * mapWidth) / convertStoP;
	sectorZSize = (SQUARE_SIZE * mapHeight) / convertStoP;

	sectorAirType.resize(sectorXSize * sectorZSize);

	circuit->LOG("  Sector-Map Block Size: %i", convertStoP);
	circuit->LOG("  Sector-Map Size: %li (x%i, z%i)", sectorXSize * sectorZSize, sectorXSize, sectorZSize);

	/*
	 *  MoveType Detection and TerrainMapMobileType Initialization
	 */
	auto defs = circuit->GetCallback()->GetUnitDefs();
	for (auto def : defs) {
		if (def->IsAbleToFly()) {

			udMobileType[def->GetUnitDefId()] = -1;

		} else if (def->GetSpeed() > .0f) {

			std::shared_ptr<MoveData> moveData(def->GetMoveData());
			float maxSlope = moveData->GetMaxSlope();
			float depth = moveData->GetDepth();
			float minWaterDepth = (moveData->GetSpeedModClass() == MoveDef::Ship) ? depth : def->GetMinWaterDepth();
			float maxWaterDepth = def->GetMaxWaterDepth();
			bool canHover = def->IsAbleToHover();
			bool canFloat = def->IsFloater();  // TODO: Remove submarines from floaters? @see CCircuitDef::isSubmarine
			STerrainMapMobileType* MT = nullptr;
			int mtIdx = 0;
			for (; (unsigned)mtIdx < mobileType.size(); ++mtIdx) {
				STerrainMapMobileType& mt = mobileType[mtIdx];
				if (((mt.maxElevation == -minWaterDepth) && (mt.maxSlope == maxSlope) && (mt.canHover == canHover) && (mt.canFloat == canFloat)) &&
					((mt.minElevation == -depth) || ((mt.canHover || mt.canFloat) && (mt.minElevation <= 0) && (-maxWaterDepth <= 0))))
				{
					MT = &mt;
					break;
				}
			}
			if (MT == nullptr) {
				STerrainMapMobileType MT2;
				mobileType.push_back(MT2);
				MT = &mobileType.back();
				mtIdx = mobileType.size() - 1;
				MT->maxSlope = maxSlope;
				MT->maxElevation = -minWaterDepth;
				MT->minElevation = -depth;
				MT->canHover = canHover;
				MT->canFloat = canFloat;
				MT->sector.resize(sectorXSize * sectorZSize);
				MT->moveData = moveData;
			} else {
				if (MT->moveData->GetCrushStrength() < moveData->GetCrushStrength()) {
					std::swap(MT->moveData, moveData);  // figured it would be easier on the pathfinder
				}
				moveData = nullptr;  // delete moveData;
			}
			MT->udCount++;
			udMobileType[def->GetUnitDefId()] = mtIdx;

		} else {

			float minWaterDepth = def->GetMinWaterDepth();
			float maxWaterDepth = def->GetMaxWaterDepth();
			bool canHover = def->IsAbleToHover();
			bool canFloat = def->IsFloater();
			STerrainMapImmobileType* IT = nullptr;
			int itIdx = 0;
			for (auto& it : immobileType) {
				if (((it.maxElevation == -minWaterDepth) && (it.canHover == canHover) && (it.canFloat == canFloat)) &&
					((it.minElevation == -maxWaterDepth) || ((it.canHover || it.canFloat) && (it.minElevation <= 0) && (-maxWaterDepth <= 0))))
				{
					IT = &it;
					break;
				}
				++itIdx;
			}
			if (IT == nullptr) {
				STerrainMapImmobileType IT2;
				immobileType.push_back(IT2);
				IT = &immobileType.back();
				itIdx = immobileType.size() - 1;
				IT->maxElevation = -minWaterDepth;
				IT->minElevation = -maxWaterDepth;
				IT->canHover = canHover;
				IT->canFloat = canFloat;
			}
			IT->udCount++;
			udImmobileType[def->GetUnitDefId()] = itIdx;
		}
	}
	utils::free_clear(defs);

	circuit->LOG("  Determining Usable Terrain for all units ...");
	/*
	 *  Setting sector & determining sectors for immobileType
	 */
	sector.resize(sectorXSize * sectorZSize);
	map->GetSlopeMap(slopeMap);
	const FloatVec& standardSlopeMap = slopeMap;
	map->GetHeightMap(areaData.heightMap);
	const FloatVec& standardHeightMap = areaData.heightMap;
	const int convertStoSM = convertStoP / 16;  // * for conversion, / for reverse conversion
	const int convertStoHM = convertStoP / 8;  // * for conversion, / for reverse conversion
	const int slopeMapXSize = sectorXSize * convertStoSM;
	const int heightMapXSize = sectorXSize * convertStoHM;

	minElevation = std::numeric_limits<float>::max();
	maxElevation = std::numeric_limits<float>::min();
	percentLand = 0.0;

	for (int z = 0; z < sectorZSize; z++) {
		for (int x = 0; x < sectorXSize; x++) {
			int i = (z * sectorXSize) + x;

			sector[i].position.x = x * convertStoP + convertStoP / 2;  // Center position of the Block
			sector[i].position.z = z * convertStoP + convertStoP / 2;  //
			sector[i].position.y = map->GetElevationAt(sector[i].position.x, sector[i].position.z);

			sectorAirType[i].S = &sector[i];

			for (auto& mt : mobileType) {
				mt.sector[i].S = &sector[i];
			}

			int iMap = ((z * convertStoSM) * slopeMapXSize) + x * convertStoSM;
			for (int zS = 0; zS < convertStoSM; zS++) {
				for (int xS = 0, iS = iMap + zS * slopeMapXSize + xS; xS < convertStoSM; xS++, iS = iMap + zS * slopeMapXSize + xS) {
					if (sector[i].maxSlope < standardSlopeMap[iS]) {
						sector[i].maxSlope = standardSlopeMap[iS];
					}
				}
			}

			iMap = ((z * convertStoHM) * heightMapXSize) + x * convertStoHM;
			sector[i].minElevation = standardHeightMap[iMap];
			sector[i].maxElevation = standardHeightMap[iMap];

			for (int zH = 0; zH < convertStoHM; zH++) {
				for (int xH = 0, iH = iMap + zH * heightMapXSize + xH; xH < convertStoHM; xH++, iH = iMap + zH * heightMapXSize + xH) {
					if (standardHeightMap[iH] >= 0) {
						sector[i].percentLand++;
						percentLand++;
					}

					if (sector[i].minElevation > standardHeightMap[iH]) {
						sector[i].minElevation = standardHeightMap[iH];
						if (minElevation > standardHeightMap[iH]) {
							minElevation = standardHeightMap[iH];
						}
					} else if (sector[i].maxElevation < standardHeightMap[iH]) {
						sector[i].maxElevation = standardHeightMap[iH];
						if (maxElevation < standardHeightMap[iH]) {
							maxElevation = standardHeightMap[iH];
						}
					}
				}
			}

			sector[i].percentLand *= 100.0 / (convertStoHM * convertStoHM);

			sector[i].isWater = (sector[i].percentLand <= 50.0);

			for (auto& it : immobileType) {
				if ((it.canHover && (it.maxElevation >= sector[i].maxElevation) && !waterIsAVoid) ||
					(it.canFloat && (it.maxElevation >= sector[i].maxElevation) && !waterIsHarmful) ||
					((it.minElevation <= sector[i].minElevation) && (it.maxElevation >= sector[i].maxElevation) && (!waterIsHarmful || (sector[i].minElevation >=0))))
				{
					it.sector[i] = &sector[i];
				}
			}
		}
	}

	percentLand *= 100.0 / (sectorXSize * convertStoHM * sectorZSize * convertStoHM);

	for (auto& it : immobileType) {
		it.typeUsable = (((100.0 * it.sector.size()) / float(sectorXSize * sectorZSize) >= 20.0) || ((double)convertStoP * convertStoP * it.sector.size() >= 1.8e7));
	}

	circuit->LOG("  Map Land Percent: %.2f%%", percentLand);
	if (percentLand < 85.0f) {
		circuit->LOG("  Water is a void: %s", waterIsAVoid ? "true" : "false");
		circuit->LOG("  Water is harmful: %s", waterIsHarmful ? "true" : "false");
	}
	circuit->LOG("  Minimum Elevation: %.2f", minElevation);
	circuit->LOG("  Maximum Elevation: %.2f", maxElevation);

	for (auto& it : immobileType) {
		std::string itText = "  Immobile-Type: Min/Max Elevation=(";
		if (it.canHover) {
			itText += "hover";
		} else if (it.canFloat || (it.minElevation < -10000)) {
			itText += "any";
		} else {
			itText += utils::float_to_string(it.minElevation/*, "%-.*G"*/);
		}
		itText += " / ";
		if (it.maxElevation < 10000) {
			itText += utils::float_to_string(it.maxElevation/*, "%-.*G"*/);
		} else {
			itText += "any";
		}
		float percentMap = (100.0 * it.sector.size()) / (sectorXSize * sectorZSize);
		itText += ")  \tIs buildable across " + utils::float_to_string(percentMap/*, "%-.4G"*/) + "%% of the map. (used by %d unit-defs)";
		circuit->LOG(itText.c_str(), it.udCount);
	}

	/*
	 *  Determine areas per mobileType
	 */
	const size_t MAMinimalSectors = 8;         // Minimal # of sector for a valid MapArea
	const float MAMinimalSectorPercent = 0.5;  // Minimal % of map for a valid MapArea
	for (auto& mt : mobileType) {
		std::ostringstream mtText;
		mtText.precision(2);
		mtText << std::fixed;

		mtText << "  Mobile-Type: Min/Max Elevation=(";
		if (mt.canFloat) {
			mtText << "any";
		} else if (mt.canHover) {
			mtText << "hover";
		} else {
			mtText << mt.minElevation;
		}
		mtText << " / ";
		if (mt.maxElevation < 10000) {
			mtText << mt.maxElevation;
		} else {
			mtText << "any";
		}
		mtText << ")  \tMax Slope=(" << mt.maxSlope << ")";
		mtText << ")  \tMove-Data used:'" << mt.moveData->GetName() << "'";

		std::deque<int> sectorSearch;
		std::set<int> sectorsRemaining;
		for (int iS = 0; iS < sectorZSize * sectorXSize; iS++) {
			if ((mt.canHover && (mt.maxElevation >= sector[iS].maxElevation) && !waterIsAVoid && ((sector[iS].maxElevation <= 0) || (mt.maxSlope >= sector[iS].maxSlope))) ||
				(mt.canFloat && (mt.maxElevation >= sector[iS].maxElevation) && !waterIsHarmful && ((sector[iS].maxElevation <= 0) || (mt.maxSlope >= sector[iS].maxSlope))) ||
				((mt.maxSlope >= sector[iS].maxSlope) && (mt.minElevation <= sector[iS].minElevation) && (mt.maxElevation >= sector[iS].maxElevation) && (!waterIsHarmful || (sector[iS].minElevation >= 0))))
			{
				sectorsRemaining.insert(iS);
			}
		}

		// Group sectors into areas
		int i, iX, iZ, areaSize = 0;  // Temp Var.
		while (!sectorsRemaining.empty() || !sectorSearch.empty()) {

			if (!sectorSearch.empty()) {
				i = sectorSearch.front();
				mt.area.back().sector[i] = &mt.sector[i];
				iX = i % sectorXSize;
				iZ = i / sectorXSize;
				if ((sectorsRemaining.find(i - 1) != sectorsRemaining.end()) && (iX > 0)) {  // Search left
					sectorSearch.push_back(i - 1);
					sectorsRemaining.erase(i - 1);
				}
				if ((sectorsRemaining.find(i + 1) != sectorsRemaining.end()) && (iX < sectorXSize - 1)) {  // Search right
					sectorSearch.push_back(i + 1);
					sectorsRemaining.erase(i + 1);
				}
				if ((sectorsRemaining.find(i - sectorXSize) != sectorsRemaining.end()) && (iZ > 0)) {  // Search up
					sectorSearch.push_back(i - sectorXSize);
					sectorsRemaining.erase(i - sectorXSize);
				}
				if ((sectorsRemaining.find(i + sectorXSize) != sectorsRemaining.end()) && (iZ < sectorZSize - 1)) {  // Search down
					sectorSearch.push_back(i + sectorXSize);
					sectorsRemaining.erase(i + sectorXSize);
				}
				sectorSearch.pop_front();

			} else {

				if ((areaSize > 0) && ((areaSize == MAP_AREA_LIST_SIZE) || (mt.area.back().sector.size() <= MAMinimalSectors) ||
					(100. * float(mt.area.back().sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
				{
					// Too many areas detected. Find, erase & ignore the smallest one that was found so far
					if (areaSize == MAP_AREA_LIST_SIZE) {
						mtText << "\nWARNING: The MapArea limit has been reached (possible error).";
					}
					decltype(mt.area)::iterator it, itArea;
					it = itArea = mt.area.begin();
					for (++it; it != mt.area.end(); ++it) {
						if (it->sector.size() < itArea->sector.size()) {
							itArea = it;
						}
					}
					mt.area.erase(itArea);
					areaSize--;
				}

				i = *sectorsRemaining.begin();
				sectorSearch.push_back(i);
				sectorsRemaining.erase(i);
				mt.area.emplace_back(&mt);
				areaSize++;
			}
		}
		if ((areaSize > 0) && ((mt.area.back().sector.size() <= MAMinimalSectors) ||
			(100.0 * float(mt.area.back().sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
		{
			areaSize--;
			mt.area.pop_back();
		}

		// Calculations
		float percentOfMap = 0.0;
		for (auto& area : mt.area) {
			for (auto& iS : area.sector) {
				iS.second->area = &area;
			}
			area.percentOfMap = (100.0 * area.sector.size()) / (sectorXSize * sectorZSize);
			if (area.percentOfMap >= 16.0 ) {  // A map area occupying 16% of the map
				area.areaUsable = true;
				mt.typeUsable = true;
			} else {
				area.areaUsable = false;
			}
			if ((mt.areaLargest == nullptr) || (mt.areaLargest->percentOfMap < area.percentOfMap)) {
				mt.areaLargest = &area;
			}

			percentOfMap += area.percentOfMap;
		}
		mtText << "  \tHas " << areaSize << " Map-Area(s) occupying " << percentOfMap << "%% of the map. (used by " << mt.udCount << " unit-defs)";
		circuit->LOG(mtText.str().c_str());
	}

	/*
	 *  Duplicate areaData
	 */
	SAreaData& nextAreaData = (pAreaData.load() == &areaData0) ? areaData1 : areaData0;
	nextAreaData.heightMapSizeX = mapWidth;
	nextAreaData.mobileType = mobileType;
	for (auto& mt : nextAreaData.mobileType) {
		mt.areaLargest = nullptr;
		for (auto& area : mt.area) {
			area.mobileType = &mt;
			for (auto& kv : area.sector) {
				kv.second = &mt.sector[kv.first];
				kv.second->area = &area;
			}
			if ((mt.areaLargest == nullptr) || (mt.areaLargest->percentOfMap < area.percentOfMap)) {
				mt.areaLargest = &area;
			}
		}
	}
	nextAreaData.immobileType = immobileType;
	nextAreaData.sector = sector;
	nextAreaData.sectorAirType = sectorAirType;
	for (int z = 0; z < sectorZSize; z++) {
		for (int x = 0; x < sectorXSize; x++) {
			int i = (z * sectorXSize) + x;
			nextAreaData.sectorAirType[i].S = &nextAreaData.sector[i];
			for (auto& mt : nextAreaData.mobileType) {
				mt.sector[i].S = &nextAreaData.sector[i];
			}
			for (auto& it : nextAreaData.immobileType) {
				if ((it.canHover && (it.maxElevation >= nextAreaData.sector[i].maxElevation) && !waterIsAVoid) ||
					(it.canFloat && (it.maxElevation >= nextAreaData.sector[i].maxElevation) && !waterIsHarmful) ||
					((it.minElevation <= nextAreaData.sector[i].minElevation) && (it.maxElevation >= nextAreaData.sector[i].maxElevation) && (!waterIsHarmful || (nextAreaData.sector[i].minElevation >=0))))
				{
					it.sector[i] = &nextAreaData.sector[i];
				}
			}
		}
	}

	scheduler->RunJobEvery(CScheduler::GameJob(&CTerrainData::EnqueueUpdate, this), AREA_UPDATE_RATE);
	scheduler->RunOnRelease(CScheduler::GameJob(&CTerrainData::DelegateAuthority, this, circuit));

#ifdef DEBUG_VIS
	debugDrawer = circuit->GetDebugDrawer();
//	std::ostringstream deb;
//	for (int iS = 0; iS < sectorXSize * sectorZSize; iS++) {
//		if (iS % sectorXSize == 0) deb << "\n";
//		if (sector[iS].maxElevation < 0.0) deb << "~";
//		else if (sector[iS].maxSlope > 0.5) deb << "^";
//		else if (sector[iS].maxSlope > 0.25) deb << "#";
//		else deb << "*";
//	}
//	for (auto& mt : mobileType) {
//		deb << "\n\n " << mt.moveData->GetName() << " h=" << mt.canHover << " f=" << mt.canFloat << " mb=" << mt.area.size();
//		for (int iS = 0; iS < sectorXSize * sectorZSize; iS++) {
//			if (iS % sectorXSize == 0) deb << "\n";
//			if (mt.sector[iS].area != nullptr) deb << "*";
//			else if (sector[iS].maxElevation < 0.0) deb << "~";
//			else if (sector[iS].maxSlope > 0.5) deb << "^";
//			else deb << "x";
//		}
//	}
//	int itId = 0;
//	for (auto& mt : immobileType) {
//		deb << "\n\n " << itId++ << " h=" << mt.canHover << " f=" << mt.canFloat << " mb=" << mt.sector.size();
//		for (int iS = 0; iS < sectorXSize * sectorZSize; iS++) {
//			if (iS % sectorXSize == 0) deb << "\n";
//			if (mt.sector.find(iS) != mt.sector.end()) deb << "*";
//			else if (sector[iS].maxElevation < 0.0) deb << "~";
//			else if (sector[iS].maxSlope > 0.5) deb << "^";
//			else deb << "x";
//		}
//	}
//	deb << "\n";
//	circuit->LOG(deb.str().c_str());
#endif

	isInitialized = true;
}

void CTerrainData::CorrectPosition(AIFloat3& position)
{
	if (position.x < 1) {
		position.x = 1;
	} else if (position.x > AIFloat3::maxxpos - 2) {
		position.x = AIFloat3::maxxpos - 2;
	}
	if (position.z < 1) {
		position.z = 1;
	} else if (position.z > AIFloat3::maxzpos - 2) {
		position.z = AIFloat3::maxzpos - 2;
	}
	// NOTE: Breaks flying and submerged units
//	position.y = map->GetElevationAt(position.x, position.z);
}

AIFloat3 CTerrainData::CorrectPosition(const AIFloat3& pos, const AIFloat3& dir, float& len)
{
	constexpr float EPS = 1e-3f;
	if ((std::fabs(dir.x) < EPS) || (std::fabs(dir.z) < EPS)) {
		AIFloat3 newPos = pos + dir * len;
		CorrectPosition(newPos);
		len = pos.distance2D(newPos);
		return newPos;
	}

	// branchless slab, @see util/math/RayBox.cpp
	float t1 = (0 - pos.x) / dir.x;
	float t2 = (AIFloat3::maxxpos - pos.x) / dir.x;

	// pos is inside box, not interested in tmin < 0
	float tmax = std::max(t1, t2);

	t1 = (0 - pos.z) / dir.z;
	t2 = (AIFloat3::maxzpos - pos.z) / dir.z;

	tmax = std::min(tmax, std::max(t1, t2));

	len = std::min(tmax * (1.f - EPS), len);

	return pos + dir * len;
}

//int CTerrainData::GetFileValue(int& fileSize, char*& file, std::string entry)
//{
//	for(size_t i = 0; i < entry.size(); i++) {
//		if (!islower(entry[i])) {
//			entry[i] = tolower(entry[i]);
//		}
//	}
//	size_t entryIndex = 0;
//	std::string entryValue = "";
//	for (int i = 0; i < fileSize; i++) {
//		if (entryIndex >= entry.size()) {
//			// Entry Found: Reading the value
//			if (file[i] >= '0' && file[i] <= '9') {
//				entryValue += file[i];
//			} else if (file[i] == ';') {
//				return atoi(entryValue.c_str());
//			}
//		} else if ((entry[entryIndex] == file[i]) || (!islower(file[i]) && (entry[entryIndex] == tolower(file[i])))) {  // the current letter matches
//			entryIndex++;
//		} else {
//			entryIndex = 0;
//		}
//	}
//	return 0;
//}

void CTerrainData::ComputeGeography2(CCircuitAI* circuit, int unitDefId)
{
	g_TerrainData = this;
	g_Circuit = circuit;
	mobileId = udMobileType[unitDefId];
	BWTA::analyze();
}

void CTerrainData::ComputeGeography(CCircuitAI* circuit, int unitDefId)
{
	Timer timer;
	timer.start();
	ComputeAltitude(circuit, unitDefId);

	ComputeAreas();

	Graph_CreateChokePoints();

	circuit->LOG("BWEM Map analyzed in %f seconds", timer.stopAndGetTime());
	circuit->GetScheduler()->RunJobAt(CScheduler::GameJob([this, circuit]() {
		for (Area::id a = 1; a <= AreasCount(); ++a) {
			for (Area::id b = 1; b < a; ++b) {
				const auto& chokes = m_ChokePointsMatrix[a][b];
				for (const ChokePoint& cp : chokes) {
					WalkPosition p = cp.Center();
					AIFloat3 pos = AIFloat3(p.x * convertStoP + convertStoP / 2, 0, p.y * convertStoP + convertStoP / 2);
					std::string cmd("ai_mrk_add:");
					cmd += utils::int_to_string(pos.x) + " " + utils::int_to_string(pos.z) + " 16 0.2 0.2 0.9 9";
					circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());
					circuit->GetDrawer()->AddPoint(pos, "choke");
				}
			}
		}
		{
			std::string cmd("ai_thr_draw:");
			circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

			cmd = utils::int_to_string(convertStoP, "ai_thr_size:%i");
			cmd += utils::float_to_string(0, " %f");
			circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());
		}
		{
			std::ostringstream cmd;
			cmd << "ai_blk_data:";
			char tmp[sectorXSize * sectorZSize] = {0};
			for (auto& f : m_RawFrontier) {
				tmp[f.second.x + sectorXSize * f.second.y] = 255;
			}
			cmd.write(&tmp[0], sectorXSize * sectorZSize);
			std::string s = cmd.str();
			circuit->GetLua()->CallRules(s.c_str(), s.size());
		}
	}), FRAMES_PER_SEC * 1);
}

void CTerrainData::ComputeAltitude(CCircuitAI* circuit, int unitDefId)
{
	STerrainMapMobileType::Id mobileId = udMobileType[unitDefId];
	STerrainMapMobileType& mt = pAreaData.load()->mobileType[mobileId];
//	const int altitude_scale = 8;	// 8 provides a pixel definition for altitude_t, since altitudes are computed from miniTiles which are 8x8 pixels

	// 1) Fill in and sort DeltasByAscendingAltitude
	const int range = std::max(sectorXSize, sectorZSize) / 2 + 3;  // TODO: Re-evaluate

	std::vector<std::pair<int2, altitude_t>> DeltasByAscendingAltitude;

	for (int dy = 0; dy <= range; ++dy) {
		for (int dx = dy; dx <= range; ++dx) {  // Only consider 1/8 of possible deltas. Other ones obtained by symmetry.
			if (dx || dy) {
				DeltasByAscendingAltitude.emplace_back(int2(dx, dy), altitude_t(0.5f + AIFloat3(dx, 0, dy).Length2D()/* * altitude_scale*/));
			}
		}
	}

	std::sort(DeltasByAscendingAltitude.begin(), DeltasByAscendingAltitude.end(),
		[](const std::pair<int2, altitude_t>& a, const std::pair<int2, altitude_t>& b) { return a.second < b.second; });

	// 2) Fill in ActiveSeaSideList, which basically contains all the seaside miniTiles (from which altitudes are to be computed)
	//    It also includes extra border-miniTiles which are considered as seaside miniTiles too.
	struct ActiveSeaSide {
		int2 origin;
		altitude_t lastAltitudeGenerated;
	};
	std::vector<ActiveSeaSide> ActiveSeaSideList;

	auto seaSide = [this, &mt](const int2 p) {
		if (mt.sector[p.x + sectorXSize * p.y].area != nullptr) {
			return false;
		}

		for (const int2 delta : {int2(0, -1), int2(-1, 0), int2(+1, 0), int2(0, +1)}) {
			const int2 np = p + delta;
			if (IsValid(np) && (mt.sector[np.x + sectorXSize * np.y].area != nullptr)) {
				return true;
			}
		}

		return false;
	};
	for (int z = -1; z <= sectorZSize; ++z) {
		for (int x = -1 ; x <= sectorXSize; ++x) {
			const int2 w(x, z);
			if (!IsValid(w) || seaSide(w)) {
				ActiveSeaSideList.push_back(ActiveSeaSide{w, 0});
			}
		}
	}

	// 3) Dijkstra's algorithm
	for (const auto& delta_altitude : DeltasByAscendingAltitude) {
		const int2 d = delta_altitude.first;
		const altitude_t altitude = delta_altitude.second;
		for (int i = 0 ; i < (int)ActiveSeaSideList.size() ; ++i) {
			ActiveSeaSide& Current = ActiveSeaSideList[i];
			if (altitude - Current.lastAltitudeGenerated >= 2/* * altitude_scale*/) {  // optimization : once a seaside miniTile verifies this condition,
				// we can throw it away as it will not generate min altitudes anymore
				std::swap(ActiveSeaSideList[i--], ActiveSeaSideList.back());
				ActiveSeaSideList.pop_back();
			} else {
				for (auto delta : { int2(d.x, d.y), int2(-d.x, d.y), int2(d.x, -d.y), int2(-d.x, -d.y),
									int2(d.y, d.x), int2(-d.y, d.x), int2(d.y, -d.x), int2(-d.y, -d.x) })
				{
					const int2 w = Current.origin + delta;
					if (IsValid(w)) {
						STerrainMapAreaSector& s = mt.sector[w.x + sectorXSize * w.y];
						if (s.S->altitude <= 0 && s.area != nullptr) {
							s.S->altitude = m_maxAltitude = Current.lastAltitudeGenerated = altitude;
						}
					}
				}
			}
		}
	}

printf("m_maxAltitude: %f\n", m_maxAltitude);
	drawMTID = mobileId;
	SAreaData& area2 = *GetNextAreaData();
	for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
		area2.sector[i].altitude = pAreaData.load()->sector[i].altitude;
	}
}

using MiniTile = STerrainMapSector;
// Helper class for void Map::ComputeAreas()
// Maintains some information about an area being computed
// A TempAreaInfo is not Valid() in two cases:
//   - a default-constructed TempAreaInfo instance is never Valid (used as a dummy value to simplify the algorithm).
//   - any other instance becomes invalid when absorbed (see Merge)
class TempAreaInfo
{
public:
						TempAreaInfo() : m_valid(false), m_id(0), m_top(0, 0), m_highestAltitude(0), m_size(0) { bwem_assert(!Valid());}
						TempAreaInfo(Area::id id, MiniTile* pMiniTile, WalkPosition pos)
							: m_valid(true), m_id(id), m_top(pos), m_highestAltitude(pMiniTile->Altitude()), m_size(0)
														{ Add(pMiniTile); bwem_assert(Valid()); }

	bool				Valid() const					{ return m_valid; }
	Area::id			Id() const						{ bwem_assert(Valid()); return m_id; }
	WalkPosition		Top() const						{ bwem_assert(Valid()); return m_top; }
	int					Size() const					{ bwem_assert(Valid()); return m_size; }
	altitude_t			HighestAltitude() const			{ bwem_assert(Valid()); return m_highestAltitude; }

	void				Add(MiniTile* pMiniTile)		{ bwem_assert(Valid()); ++m_size; pMiniTile->SetAreaId(m_id); }

	// Left to caller : m.SetAreaId(this->Id()) for each MiniTile m in Absorbed
	void				Merge(TempAreaInfo & Absorbed)	{
															bwem_assert(Valid() && Absorbed.Valid());
															bwem_assert(m_size >= Absorbed.m_size);
															m_size += Absorbed.m_size;
															Absorbed.m_valid = false;
														}

	TempAreaInfo &		operator=(const TempAreaInfo&) = delete;

private:
	bool				m_valid;
	const Area::id		m_id;
	const WalkPosition	m_top;
	const altitude_t	m_highestAltitude;
	int					m_size;
};

// Assigns MiniTile::m_areaId for each miniTile having AreaIdMissing()
// Areas are computed using MiniTile::Altitude() information only.
// The miniTiles are considered successively in descending order of their Altitude().
// Each of them either:
//   - involves the creation of a new area.
//   - is added to some existing neighbouring area.
//   - makes two neighbouring areas merge together.
void CTerrainData::ComputeAreas()
{
	std::vector<std::pair<WalkPosition, MiniTile*>> MiniTilesByDescendingAltitude = SortMiniTiles();

	std::vector<TempAreaInfo> TempAreaList = ComputeTempAreas(MiniTilesByDescendingAltitude);

	CreateAreas(TempAreaList);

//	SetAreaIdInTiles();
}

std::vector<std::pair<WalkPosition, MiniTile*>> CTerrainData::SortMiniTiles()
{
	std::vector<std::pair<WalkPosition, MiniTile*>> MiniTilesByDescendingAltitude;
	for (int y = 0; y < sectorZSize; ++y) {
		for (int x = 0; x < sectorXSize; ++x) {
			WalkPosition w = WalkPosition(x, y);
			MiniTile& miniTile = GetMiniTile(w);
			if (miniTile.area_id <= 0 && miniTile.altitude > 0) {
				MiniTilesByDescendingAltitude.emplace_back(w, &miniTile);
			}
		}
	}

	std::sort(MiniTilesByDescendingAltitude.begin(), MiniTilesByDescendingAltitude.end(),
		[](const std::pair<WalkPosition, MiniTile*>& a, const std::pair<WalkPosition, MiniTile*>& b) { return a.second->Altitude() > b.second->Altitude(); });

	return MiniTilesByDescendingAltitude;
}

static std::pair<Area::id, Area::id> findNeighboringAreas(WalkPosition p, const CTerrainData* pMap)
{
	std::pair<Area::id, Area::id> result(0, 0);

	for (WalkPosition delta : {WalkPosition(0, -1), WalkPosition(-1, 0), WalkPosition(+1, 0), WalkPosition(0, +1)}) {
		if (pMap->IsValid(p + delta)) {
			Area::id areaId = pMap->GetMiniTile(p + delta).AreaId();
			if (areaId > 0) {
				if (!result.first) result.first = areaId;
				else if (result.first != areaId)
					if (!result.second || ((areaId < result.second)))
						result.second = areaId;
			}
		}
	}

	return result;
}

static Area::id chooseNeighboringArea(Area::id a, Area::id b)
{
	static std::map<std::pair<Area::id, Area::id>, int> map_AreaPair_counter;

	if (a > b) std::swap(a, b);
	return (map_AreaPair_counter[std::make_pair(a, b)]++ % 2 == 0) ? a : b;
}

std::vector<TempAreaInfo> CTerrainData::ComputeTempAreas(const std::vector<std::pair<WalkPosition, MiniTile*>>& MiniTilesByDescendingAltitude)
{
	std::vector<TempAreaInfo> TempAreaList(1);  // TempAreaList[0] left unused, as AreaIds are > 0
	for (const auto& Current : MiniTilesByDescendingAltitude) {
		const WalkPosition pos = Current.first;
		MiniTile* cur = Current.second;

		std::pair<Area::id, Area::id> neighboringAreas = findNeighboringAreas(pos, this);
		if (!neighboringAreas.first) {  // no neighboring area : creates of a new area
			TempAreaList.emplace_back((Area::id)TempAreaList.size(), cur, pos);
		} else if (!neighboringAreas.second) {  // one neighboring area : adds cur to the existing area
			TempAreaList[neighboringAreas.first].Add(cur);
		} else {  // two neighboring areas : adds cur to one of them  &  possible merging
			Area::id smaller = neighboringAreas.first;
			Area::id bigger = neighboringAreas.second;
			if (TempAreaList[smaller].Size() > TempAreaList[bigger].Size()) std::swap(smaller, bigger);

			// Condition for the neighboring areas to merge:
			if ((TempAreaList[smaller].Size() < 20) ||  // was 80
				(TempAreaList[smaller].HighestAltitude() < 2.5) ||  // was 80 == 10*altitude_scale, using Tiles = 4x4 MiniTiles
				(cur->Altitude() / (float)TempAreaList[bigger].HighestAltitude() >= 0.90) ||
				(cur->Altitude() / (float)TempAreaList[smaller].HighestAltitude() >= 0.90) ||
//				any_of(StartingLocations().begin(), StartingLocations().end(), [&pos](const TilePosition & startingLoc)
//					{ return dist(TilePosition(pos), startingLoc + TilePosition(2, 1)) <= 3;}) ||
				false
				)
			{
				// adds cur to the absorbing area:
				TempAreaList[bigger].Add(cur);

				// merges the two neighboring areas:
				ReplaceAreaIds(TempAreaList[smaller].Top(), bigger);
				TempAreaList[bigger].Merge(TempAreaList[smaller]);
			}
			else	// no merge : cur starts or continues the frontier between the two neighboring areas
			{
				// adds cur to the chosen Area:
				TempAreaList[chooseNeighboringArea(smaller, bigger)].Add(cur);
				m_RawFrontier.emplace_back(neighboringAreas, pos);
			}
		}
	}

	// Remove from the frontier obsolete positions
	m_RawFrontier.erase(std::remove_if(m_RawFrontier.begin(), m_RawFrontier.end(),
			[](const std::pair<std::pair<Area::id, Area::id>, WalkPosition>& f) { return f.first.first == f.first.second; }
	), m_RawFrontier.end());

	return TempAreaList;
}

void CTerrainData::ReplaceAreaIds(WalkPosition p, Area::id newAreaId)
{
	MiniTile& Origin = GetMiniTile(p);
	Area::id oldAreaId = Origin.AreaId();
	Origin.ReplaceAreaId(newAreaId);

	std::vector<WalkPosition> ToSearch{p};
	while (!ToSearch.empty()) {
		WalkPosition current = ToSearch.back();

		ToSearch.pop_back();
		for (WalkPosition delta : {WalkPosition(0, -1), WalkPosition(-1, 0), WalkPosition(+1, 0), WalkPosition(0, +1)}) {
			WalkPosition next = current + delta;
			if (IsValid(next)) {
				MiniTile& Next = GetMiniTile(next);
				if (Next.AreaId() == oldAreaId) {
					ToSearch.push_back(next);
					Next.ReplaceAreaId(newAreaId);
				}
			}
		}
	}

	// also replaces references of oldAreaId by newAreaId in m_RawFrontier:
	if (newAreaId > 0) {
		for (auto& f : m_RawFrontier) {
			if (f.first.first == oldAreaId) f.first.first = newAreaId;
			if (f.first.second == oldAreaId) f.first.second = newAreaId;
		}
	}
}

// Initializes m_Graph with the valid and big enough areas in TempAreaList.
void CTerrainData::CreateAreas(const std::vector<TempAreaInfo>& TempAreaList)
{
	constexpr int area_min_miniTiles = 32;  // was 64

	typedef std::pair<WalkPosition, int> pair_top_size_t;
	std::vector<pair_top_size_t> AreasList;

	Area::id newAreaId = 1;
	Area::id newTinyAreaId = -2;

	for (auto & TempArea : TempAreaList) {
		if (TempArea.Valid()) {
			if (TempArea.Size() >= area_min_miniTiles) {
				bwem_assert(newAreaId <= TempArea.Id());
				if (newAreaId != TempArea.Id()) {
					ReplaceAreaIds(TempArea.Top(), newAreaId);
				}

				AreasList.emplace_back(TempArea.Top(), TempArea.Size());
				newAreaId++;
			} else {
				ReplaceAreaIds(TempArea.Top(), newTinyAreaId);
				newTinyAreaId--;
			}
		}
	}

	Graph_CreateAreas(AreasList);
}

void CTerrainData::Graph_CreateAreas(const std::vector<std::pair<WalkPosition, int>>& AreasList)
{
	m_Areas.reserve(AreasList.size());
	for (Area::id id = 1; id <= (Area::id)AreasList.size(); ++id) {
		WalkPosition top = AreasList[id - 1].first;
		int miniTiles = AreasList[id - 1].second;
		m_Areas.emplace_back(this, id, top, miniTiles);
	}
}

inline int queenWiseNorm(int dx, int dy) { return std::max(abs(dx), abs(dy)); }

//template<typename T, int Scale = 1>
//inline int queenWiseDist(BWAPI::Point<T, Scale> A, BWAPI::Point<T, Scale> B){ A -= B; return utils::queenWiseNorm(A.x, A.y); }
inline int queenWiseDist(WalkPosition A, WalkPosition B){ A -= B; return queenWiseNorm(A.x, A.y); }

/*const */std::vector<ChokePoint>& CTerrainData::GetChokePoints(Area::id a, Area::id b)/* const*/
{
	bwem_assert(Valid(a));
	bwem_assert(Valid(b));
	bwem_assert(a != b);

	if (a > b) std::swap(a, b);

	return m_ChokePointsMatrix[b][a];
}

void CTerrainData::Graph_CreateChokePoints()
{
	constexpr int lake_max_miniTiles = 8;  // was 64

	ChokePoint::index newIndex = 0;

//	vector<Neutral *> BlockingNeutrals;
//	for (auto & s : GetMap()->StaticBuildings())		if (s->Blocking()) BlockingNeutrals.push_back(s.get());
//	for (auto & m : GetMap()->Minerals())			if (m->Blocking()) BlockingNeutrals.push_back(m.get());
//
//	const int pseudoChokePointsToCreate = count_if(BlockingNeutrals.begin(), BlockingNeutrals.end(),
//											[](const Neutral * n){ return !n->NextStacked(); });

	// 1) Size the matrix
	m_ChokePointsMatrix.resize(AreasCount() + 1);
	for (Area::id id = 1 ; id <= AreasCount() ; ++id) {
		m_ChokePointsMatrix[id].resize(id);			// triangular matrix
	}

	// 2) Dispatch the global raw frontier between all the relevant pairs of Areas:
	std::map<std::pair<Area::id, Area::id>, std::vector<WalkPosition>> RawFrontierByAreaPair;
	for (const auto& raw : m_RawFrontier) {
		Area::id a = raw.first.first;
		Area::id b = raw.first.second;
		if (a > b) std::swap(a, b);
		bwem_assert(a <= b);
		bwem_assert((a >= 1) && (b <= AreasCount()));

		RawFrontierByAreaPair[std::make_pair(a, b)].push_back(raw.second);
	}

	// 3) For each pair of Areas (A, B):
	for (auto& raw : RawFrontierByAreaPair) {
		Area::id a = raw.first.first;
		Area::id b = raw.first.second;

		const std::vector<WalkPosition>& RawFrontierAB = raw.second;

		// Because our dispatching preserved order,
		// and because Map::m_RawFrontier was populated in descending order of the altitude (see Map::ComputeAreas),
		// we know that RawFrontierAB is also ordered the same way, but let's check it:
		{
			std::vector<altitude_t> Altitudes;
			for (auto w : RawFrontierAB) {
				Altitudes.push_back(GetMiniTile(w).Altitude());
			}

			bwem_assert(is_sorted(Altitudes.rbegin(), Altitudes.rend()));
		}

		// 3.1) Use that information to efficiently cluster RawFrontierAB in one or several chokepoints.
		//    Each cluster will be populated starting with the center of a chokepoint (max altitude)
		//    and finishing with the ends (min altitude).
		const int cluster_min_dist = (int)sqrt(lake_max_miniTiles);
		std::vector<std::deque<WalkPosition>> Clusters;
		for (auto w : RawFrontierAB) {
			bool added = false;
			for (auto & Cluster : Clusters) {
				int distToFront = queenWiseDist(Cluster.front(), w);
				int distToBack = queenWiseDist(Cluster.back(), w);
				if (std::min(distToFront, distToBack) <= cluster_min_dist) {
					if (distToFront < distToBack)	Cluster.push_front(w);
					else							Cluster.push_back(w);

					added = true;
					break;
				}
			}

			if (!added) Clusters.push_back(std::deque<WalkPosition>(1, w));
		}

		// 3.2) Create one Chokepoint for each cluster:
		GetChokePoints(a, b).reserve(Clusters.size()/* + pseudoChokePointsToCreate*/);
		for (const auto & Cluster : Clusters)
			GetChokePoints(a, b).emplace_back(this, newIndex++, /*GetArea(a)*/a, /*GetArea(b)*/b, Cluster);
	}

	// 4) Create one Chokepoint for each pair of blocked areas, for each blocking Neutral:
//	for (Neutral * pNeutral : BlockingNeutrals)
//		if (!pNeutral->NextStacked())		// in the case where several neutrals are stacked, we only consider the top
//		{
//			vector<const Area *> BlockedAreas = pNeutral->BlockedAreas();
//			for (const Area * pA : BlockedAreas)
//			for (const Area * pB : BlockedAreas)
//			{
//				if (pB == pA) break;	// breaks symmetry
//
//				auto center = GetMap()->BreadthFirstSearch(WalkPosition(pNeutral->Pos()),
//						[](const MiniTile & miniTile, WalkPosition) { return miniTile.Walkable(); },	// findCond
//						[](const MiniTile &,          WalkPosition) { return true; });					// visitCond
//
//				GetChokePoints(pA, pB).reserve(pseudoChokePointsToCreate);
//				GetChokePoints(pA, pB).emplace_back(this, newIndex++, pA, pB, deque<WalkPosition>(1, center), pNeutral);
//			}
//		}

	// 5) Set the references to the freshly created Chokepoints:
//	for (Area::id a = 1 ; a <= AreasCount() ; ++a)
//	for (Area::id b = 1 ; b < a ; ++b)
//		if (!GetChokePoints(a, b).empty())
//		{
//			GetArea(a)->AddChokePoints(GetArea(b), &GetChokePoints(a, b));
//			GetArea(b)->AddChokePoints(GetArea(a), &GetChokePoints(a, b));
//
//			for (auto & cp : GetChokePoints(a, b))
//				m_ChokePointList.push_back(&cp);
//		}
}

void CTerrainData::DelegateAuthority(CCircuitAI* curOwner)
{
	for (CCircuitAI* circuit : gameAttribute->GetCircuits()) {
		if (circuit->IsInitialized() && (circuit != curOwner)) {
			map = circuit->GetMap();
			scheduler = circuit->GetScheduler();
			scheduler->RunJobEvery(CScheduler::GameJob(&CTerrainData::EnqueueUpdate, this), AREA_UPDATE_RATE);
			scheduler->RunJobAfter(CScheduler::GameJob(&CTerrainData::EnqueueUpdate, this), FRAMES_PER_SEC);
			scheduler->RunOnRelease(CScheduler::GameJob(&CTerrainData::DelegateAuthority, this, circuit));
			break;
		}
	}
}

void CTerrainData::EnqueueUpdate()
{
	SCOPED_TIME(*gameAttribute->GetCircuits().begin(), __PRETTY_FUNCTION__);
	if (isUpdating) {
		return;
	}
	isUpdating = true;

	map->GetHeightMap(GetNextAreaData()->heightMap);
	map->GetSlopeMap(slopeMap);

	scheduler->RunPriorityJob(CScheduler::WorkJob(&CTerrainData::UpdateAreas, this));
}

std::shared_ptr<IMainJob> CTerrainData::UpdateAreas()
{
	/*
	 *  Assign areaData references
	 */
	SAreaData& areaData = *GetNextAreaData();
	std::vector<STerrainMapMobileType>& mobileType = areaData.mobileType;
	std::vector<STerrainMapImmobileType>& immobileType = areaData.immobileType;
	std::vector<STerrainMapSector>& sector = areaData.sector;
	float& minElevation = areaData.minElevation;
	float& maxElevation = areaData.maxElevation;
	float& percentLand = areaData.percentLand;

	/*
	 *  Reset areaData
	 */
	SAreaData& prevAreaData = *pAreaData.load();
	decltype(areaData.sector)::iterator its = sector.begin();
	for (auto& s : prevAreaData.sector) {
		*its++ = s;
	}
	decltype(areaData.mobileType)::iterator itmt = mobileType.begin();
	for (unsigned i = 0; i < prevAreaData.mobileType.size(); ++i) {
		itmt->areaLargest = nullptr;
		for (auto& as : itmt->sector) {
			as.area = nullptr;
			// TODO: Use previous sectorAlternativeM/I and update it?
			as.sectorAlternativeM.clear();
			as.sectorAlternativeI.clear();
		}
		itmt->area.clear();
		++itmt;
	}
	decltype(areaData.immobileType)::iterator itit = immobileType.begin();
	for (auto& it : prevAreaData.immobileType) {
		itit->sector.clear();
		for (auto& kv : it.sector) {
			itit->sector[kv.first] = &sector[kv.first];
		}
		// TODO: Use previous sectorClosest and update it (replace STerrainMapSector* by index)?
		itit->sectorClosest.clear();
		++itit;
	}
	minElevation = prevAreaData.minElevation;
	maxElevation = prevAreaData.maxElevation;
	percentLand = prevAreaData.percentLand;

	/*
	 *  Updating sector & determining sectors for immobileType
	 */
	const FloatVec& standardSlopeMap = slopeMap;
	const FloatVec& standardHeightMap = areaData.heightMap;
	const FloatVec& prevHeightMap = prevAreaData.heightMap;
	const int convertStoSM = convertStoP / 16;  // * for conversion, / for reverse conversion
	const int convertStoHM = convertStoP / 8;  // * for conversion, / for reverse conversion
	const int slopeMapXSize = sectorXSize * convertStoSM;
	const int heightMapXSize = sectorXSize * convertStoHM;

	float tmpPercentLand = std::round(percentLand * (sectorXSize * convertStoHM * sectorZSize * convertStoHM) / 100.0);

	auto isSectorHeightChanged = [&standardHeightMap, &prevHeightMap, convertStoHM, heightMapXSize](int iMap) {
		for (int zH = 0; zH < convertStoHM; zH++) {
			for (int xH = 0, iH = iMap + zH * heightMapXSize + xH; xH < convertStoHM; xH++, iH = iMap + zH * heightMapXSize + xH) {
				if (standardHeightMap[iH] != prevHeightMap[iH]) {
					return true;
				}
			}
		}
		return false;
	};
	std::set<int> changedSectors;

	for (int z = 0; z < sectorZSize; z++) {
		for (int x = 0; x < sectorXSize; x++) {
			int iMapH = ((z * convertStoHM) * heightMapXSize) + x * convertStoHM;
			if (!isSectorHeightChanged(iMapH)) {
				continue;
			}

			int i = (z * sectorXSize) + x;
			changedSectors.insert(i);

			int xi = sector[i].position.x / SQUARE_SIZE;
			int zi = sector[i].position.z / SQUARE_SIZE;
			sector[i].position.y = standardHeightMap[zi * heightMapXSize + xi];

			sector[i].maxSlope = .0f;
			int iMapS = ((z * convertStoSM) * slopeMapXSize) + x * convertStoSM;
			for (int zS = 0; zS < convertStoSM; zS++) {
				for (int xS = 0, iS = iMapS + zS * slopeMapXSize + xS; xS < convertStoSM; xS++, iS = iMapS + zS * slopeMapXSize + xS) {
					if (sector[i].maxSlope < standardSlopeMap[iS]) {
						sector[i].maxSlope = standardSlopeMap[iS];
					}
				}
			}

			float prevPercentLand = std::round(sector[i].percentLand * (convertStoHM * convertStoHM) / 100.0);
			sector[i].percentLand = .0f;
			sector[i].minElevation = standardHeightMap[iMapH];
			sector[i].maxElevation = standardHeightMap[iMapH];
			for (int zH = 0; zH < convertStoHM; zH++) {
				for (int xH = 0, iH = iMapH + zH * heightMapXSize + xH; xH < convertStoHM; xH++, iH = iMapH + zH * heightMapXSize + xH) {
					if (standardHeightMap[iH] >= 0) {
						sector[i].percentLand++;
					}

					if (sector[i].minElevation > standardHeightMap[iH]) {
						sector[i].minElevation = standardHeightMap[iH];
						if (minElevation > standardHeightMap[iH]) {
							minElevation = standardHeightMap[iH];
						}
					} else if (sector[i].maxElevation < standardHeightMap[iH]) {
						sector[i].maxElevation = standardHeightMap[iH];
						if (maxElevation < standardHeightMap[iH]) {
							maxElevation = standardHeightMap[iH];
						}
					}
				}
			}

			if (sector[i].percentLand != prevPercentLand) {
				tmpPercentLand += sector[i].percentLand - prevPercentLand;
			}
			sector[i].percentLand *= 100.0 / (convertStoHM * convertStoHM);

			sector[i].isWater = (sector[i].percentLand <= 50.0);

			for (auto& it : immobileType) {
				if ((it.canHover && (it.maxElevation >= sector[i].maxElevation) && !waterIsAVoid) ||
					(it.canFloat && (it.maxElevation >= sector[i].maxElevation) && !waterIsHarmful) ||
					((it.minElevation <= sector[i].minElevation) && (it.maxElevation >= sector[i].maxElevation) && (!waterIsHarmful || (sector[i].minElevation >= 0))))
				{
					it.sector[i] = &sector[i];
				} else {
					it.sector.erase(i);
				}
			}
		}
	}

	percentLand = tmpPercentLand * 100.0 / (sectorXSize * convertStoHM * sectorZSize * convertStoHM);

	for (auto& it : immobileType) {
		it.typeUsable = (((100.0 * it.sector.size()) / float(sectorXSize * sectorZSize) >= 20.0) || ((double)convertStoP * convertStoP * it.sector.size() >= 1.8e7));
	}

	/*
	 *  Determine areas per mobileType
	 */
	auto shouldRebuild = [this, &changedSectors, &sector](STerrainMapMobileType& mt) {
		for (auto iS : changedSectors) {
			if ((mt.canHover && (mt.maxElevation >= sector[iS].maxElevation) && !waterIsAVoid && ((sector[iS].maxElevation <= 0) || (mt.maxSlope >= sector[iS].maxSlope))) ||
				(mt.canFloat && (mt.maxElevation >= sector[iS].maxElevation) && !waterIsHarmful && ((sector[iS].maxElevation <= 0) || (mt.maxSlope >= sector[iS].maxSlope))) ||
				((mt.maxSlope >= sector[iS].maxSlope) && (mt.minElevation <= sector[iS].minElevation) && (mt.maxElevation >= sector[iS].maxElevation) && (!waterIsHarmful || (sector[iS].minElevation >= 0))))
			{
				if (mt.sector[iS].area == nullptr) {
					return true;
				}
			} else {
				if (mt.sector[iS].area != nullptr) {
					return true;
				}
			}
		}
		return false;
	};
	const size_t MAMinimalSectors = 8;         // Minimal # of sector for a valid MapArea
	const float MAMinimalSectorPercent = 0.5;  // Minimal % of map for a valid MapArea
	itmt = prevAreaData.mobileType.begin();
	for (auto& mt : mobileType) {
		if (shouldRebuild(*itmt)) {

			std::deque<int> sectorSearch;
			std::set<int> sectorsRemaining;
			for (int iS = 0; iS < sectorZSize * sectorXSize; iS++) {
				if ((mt.canHover && (mt.maxElevation >= sector[iS].maxElevation) && !waterIsAVoid && ((sector[iS].maxElevation <= 0) || (mt.maxSlope >= sector[iS].maxSlope))) ||
					(mt.canFloat && (mt.maxElevation >= sector[iS].maxElevation) && !waterIsHarmful && ((sector[iS].maxElevation <= 0) || (mt.maxSlope >= sector[iS].maxSlope))) ||
					((mt.maxSlope >= sector[iS].maxSlope) && (mt.minElevation <= sector[iS].minElevation) && (mt.maxElevation >= sector[iS].maxElevation) && (!waterIsHarmful || (sector[iS].minElevation >= 0))))
				{
					sectorsRemaining.insert(iS);
				}
			}

			// Group sectors into areas
			int i, iX, iZ, areaSize = 0;  // Temp Var.
			while (!sectorsRemaining.empty() || !sectorSearch.empty()) {

				if (!sectorSearch.empty()) {
					i = sectorSearch.front();
					mt.area.back().sector[i] = &mt.sector[i];
					iX = i % sectorXSize;
					iZ = i / sectorXSize;
					if ((sectorsRemaining.find(i - 1) != sectorsRemaining.end()) && (iX > 0)) {  // Search left
						sectorSearch.push_back(i - 1);
						sectorsRemaining.erase(i - 1);
					}
					if ((sectorsRemaining.find(i + 1) != sectorsRemaining.end()) && (iX < sectorXSize - 1)) {  // Search right
						sectorSearch.push_back(i + 1);
						sectorsRemaining.erase(i + 1);
					}
					if ((sectorsRemaining.find(i - sectorXSize) != sectorsRemaining.end()) && (iZ > 0)) {  // Search up
						sectorSearch.push_back(i - sectorXSize);
						sectorsRemaining.erase(i - sectorXSize);
					}
					if ((sectorsRemaining.find(i + sectorXSize) != sectorsRemaining.end()) && (iZ < sectorZSize - 1)) {  // Search down
						sectorSearch.push_back(i + sectorXSize);
						sectorsRemaining.erase(i + sectorXSize);
					}
					sectorSearch.pop_front();

				} else {

					if ((areaSize > 0) && ((areaSize == MAP_AREA_LIST_SIZE) || (mt.area.back().sector.size() <= MAMinimalSectors) ||
						(100. * float(mt.area.back().sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
					{
						decltype(mt.area)::iterator it, itArea;
						it = itArea = mt.area.begin();
						for (++it; it != mt.area.end(); ++it) {
							if (it->sector.size() < itArea->sector.size()) {
								itArea = it;
							}
						}
						mt.area.erase(itArea);
						areaSize--;
					}

					i = *sectorsRemaining.begin();
					sectorSearch.push_back(i);
					sectorsRemaining.erase(i);
					mt.area.emplace_back(&mt);
					areaSize++;
				}
			}
			if ((areaSize > 0) && ((mt.area.back().sector.size() <= MAMinimalSectors) ||
				(100.0 * float(mt.area.back().sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
			{
				mt.area.pop_back();
			}

		} else {  // should not rebuild

			// Copy mt.area from previous areaData
			for (auto& area : itmt->area) {
				mt.area.emplace_back(&mt);
				std::map<int, STerrainMapAreaSector*>& sector = mt.area.back().sector;
				for (auto& kv : area.sector) {
					sector[kv.first] = &mt.sector[kv.first];
				}
			}
		}

		// Calculations
		for (auto& area : mt.area) {
			for (auto& iS : area.sector) {
				iS.second->area = &area;
			}
			area.percentOfMap = (100.0 * area.sector.size()) / (sectorXSize * sectorZSize);
			if (area.percentOfMap >= 16.0 ) {  // A map area occupying 16% of the map
				area.areaUsable = true;
				mt.typeUsable = true;
			} else {
				area.areaUsable = false;
			}
			if ((mt.areaLargest == nullptr) || (mt.areaLargest->percentOfMap < area.percentOfMap)) {
				mt.areaLargest = &area;
			}
		}

		++itmt;
	}

	return CScheduler::GameJob(&CTerrainData::ScheduleUsersUpdate, this);
}

void CTerrainData::ScheduleUsersUpdate()
{
	aiToUpdate = 0;
	const int interval = gameAttribute->GetCircuits().size();
	for (CCircuitAI* circuit : gameAttribute->GetCircuits()) {
		if (circuit->IsInitialized()) {
			// Chain update: CTerrainManager -> CBuilderManager -> CPathFinder
			auto task = CScheduler::GameJob(&CTerrainManager::UpdateAreaUsers,
											circuit->GetTerrainManager(),
											interval);
			circuit->GetScheduler()->RunJobAfter(task, ++aiToUpdate);
			circuit->PrepareAreaUpdate();
		}
	}
	// Check if there are any ai to update
	++aiToUpdate;
	OnAreaUsersUpdated();
}

void CTerrainData::OnAreaUsersUpdated()
{
	if (--aiToUpdate != 0) {
		return;
	}

	pAreaData = GetNextAreaData();
	isUpdating = false;

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

//void CTerrainData::DrawConvexHulls(Drawer* drawer)
//{
//	for (const MetalIndices& indices : GetClusters()) {
//		if (indices.empty()) {
//			continue;
//		} else if (indices.size() == 1) {
//			drawer->AddPoint(spots[indices[0]].position, "Cluster 1");
//		} else if (indices.size() == 2) {
//			drawer->AddLine(spots[indices[0]].position, spots[indices[1]].position);
//		} else {
//			// !!! Graham scan !!!
//			// Coord system:  *-----x
//			//                |
//			//                |
//			//                z
//			auto orientation = [](const AIFloat3& p1, const AIFloat3& p2, const AIFloat3& p3) {
//				// orientation > 0 : counter-clockwise turn,
//				// orientation < 0 : clockwise,
//				// orientation = 0 : collinear
//				return (p2.x - p1.x) * (p3.z - p1.z) - (p2.z - p1.z) * (p3.x - p1.x);
//			};
//			// number of points
//			int N = indices.size();
//			// the array of points
//			std::vector<AIFloat3> points(N + 1);
//			// Find the bottom-most point
//			int min = 1, i = 1;
//			float zmin = spots[indices[0]].position.z;
//			for (const int idx : indices) {
//				points[i] = spots[idx].position;
//				float z = spots[idx].position.z;
//				// Pick the bottom-most or chose the left most point in case of tie
//				if ((z < zmin) || (zmin == z && points[i].x < points[min].x)) {
//					zmin = z, min = i;
//				}
//				i++;
//			}
//			auto swap = [](AIFloat3& p1, AIFloat3& p2) {
//				AIFloat3 tmp = p1;
//				p1 = p2;
//				p2 = tmp;
//			};
//			swap(points[1], points[min]);
//
//			// A function used to sort an array of
//			// points with respect to the first point
//			AIFloat3& p0 = points[1];
//			auto compare = [&p0, orientation](const AIFloat3& p1, const AIFloat3& p2) {
//				// Find orientation
//				int o = orientation(p0, p1, p2);
//				if (o == 0) {
//					return p0.SqDistance2D(p1) < p0.SqDistance2D(p2);
//				}
//				return o > 0;
//			};
//			// Sort n-1 points with respect to the first point. A point p1 comes
//			// before p2 in sorted output if p2 has larger polar angle (in
//			// counterclockwise direction) than p1
//			std::sort(points.begin() + 2, points.end(), compare);
//
//			// let points[0] be a sentinel point that will stop the loop
//			points[0] = points[N];
//
////			int M = 1; // Number of points on the convex hull.
////			for (int i(2); i <= N; ++i) {
////				while (orientation(points[M - 1], points[M], points[i]) <= 0) {
////					if (M > 1) {
////						M--;
////					} else if (i == N) {
////						break;
////					} else {
////						i++;
////					}
////				}
////				swap(points[++M], points[i]);
////			}
//
//			int M = N;  // FIXME: Remove this DEBUG line
//			// draw convex hull
//			AIFloat3 start = points[0], end;
//			for (int i = 1; i < M; i++) {
//				end = points[i];
//				drawer->AddLine(start, end);
//				start = end;
//			}
//			end = points[0];
//			drawer->AddLine(start, end);
//		}
//	}
//}

//void CMetalManager::DrawCentroids(Drawer* drawer)
//{
//	for (int i = 0; i < metalCluster.size(); i++) {
//		std::string msgText = utils::string_format("%i mexes cluster", metalCluster[i].size());
//		drawer->AddPoint(centroids[i], msgText.c_str());
//	}
//}

//void CTerrainData::ClearMetalClusters(Drawer* drawer)
//{
//	for (auto& cluster : GetClusters()) {
//		for (auto& idx : cluster) {
//			drawer->DeletePointsAndLines(spots[idx].position);
//		}
//	}
////	clusters.clear();
////
////	for (auto& centroid : centroids) {
////		drawer->DeletePointsAndLines(centroid);
////	}
////	centroids.clear();
//}

#ifdef DEBUG_VIS
#define WATER(x, i) {	\
	x[i * 3 + 0] = .2f;  /*R*/	\
	x[i * 3 + 1] = .2f;  /*G*/	\
	x[i * 3 + 2] = .8f;  /*B*/	\
}
#define HILL(x, i) {	\
	x[i * 3 + 0] = .65f;  /*R*/	\
	x[i * 3 + 1] = .16f;  /*G*/	\
	x[i * 3 + 2] = .16f;  /*B*/	\
}
#define LAND(x, i) {	\
	x[i * 3 + 0] = .2f;  /*R*/	\
	x[i * 3 + 1] = .8f;  /*G*/	\
	x[i * 3 + 2] = .2f;  /*B*/	\
}
#define MOUNTAIN(x, i) {	\
	x[i * 3 + 0] = 1.f;  /*R*/	\
	x[i * 3 + 1] = .0f;  /*G*/	\
	x[i * 3 + 2] = .0f;  /*B*/	\
}
#define BLOCK(x, i) {	\
	x[i * 3 + 0] = .0f;  /*R*/	\
	x[i * 3 + 1] = .0f;  /*G*/	\
	x[i * 3 + 2] = .0f;  /*B*/	\
}

void CTerrainData::UpdateVis()
{
	if ((debugDrawer == nullptr) || sdlWindows.empty()) {
		return;
	}

	SAreaData& areaData = *GetNextAreaData();
	std::vector<STerrainMapSector>& sector = areaData.sector;
	int winNum = 0;

	std::pair<Uint32, float*> win = sdlWindows[winNum++];
	for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
		if (sector[i].maxElevation < 0.0) WATER(win.second, i)
		else if (sector[i].maxSlope > 0.5) MOUNTAIN(win.second, i)
		else if (sector[i].maxSlope > 0.25) HILL(win.second, i)
		else LAND(win.second, i)
	}
	debugDrawer->DrawTex(win.first, win.second);

//	for (const STerrainMapMobileType& mt : areaData.mobileType) {
//		std::pair<Uint32, float*> win = sdlWindows[winNum++];
//		for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
//			if (mt.sector[i].area != nullptr) LAND(win.second, i)
//			else if (sector[i].maxElevation < 0.0) WATER(win.second, i)
//			else if (sector[i].maxSlope > 0.5) HILL(win.second, i)
//			else BLOCK(win.second, i)
//		}
//		debugDrawer->DrawTex(win.first, win.second);
//	}
	int id = 0;
	for (const STerrainMapMobileType& mt : areaData.mobileType) {
		if (id++ == drawMTID) {
			std::pair<Uint32, float*> win = sdlWindows[winNum++];
			for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
//				win.second[i * 3 + 0] = (1.f - areaData.sector[i].altitude / m_maxAltitude) * 0.2f;
//				win.second[i * 3 + 1] = (1.f - areaData.sector[i].altitude / m_maxAltitude) * 0.8f;
//				win.second[i * 3 + 2] = (1.f - areaData.sector[i].altitude / m_maxAltitude) * 0.2f;
				if (areaData.sector[i].altitude >= 0) {
					win.second[i * 3 + 0] = areaData.sector[i].altitude / m_maxAltitude * 0.2f;
					win.second[i * 3 + 1] = areaData.sector[i].altitude / m_maxAltitude * 1.0f;
					win.second[i * 3 + 2] = areaData.sector[i].altitude / m_maxAltitude * 0.2f;
				} else {
					win.second[i * 3 + 0] = 0.5f;
					win.second[i * 3 + 1] = 0.5f;
					win.second[i * 3 + 2] = 0.5f;
				}
			}
			for (auto& f : m_RawFrontier) {
				int i = f.second.x + sectorXSize * f.second.y;
				win.second[i * 3 + 0] = 0.9f;
				win.second[i * 3 + 1] = 0.9f;
				win.second[i * 3 + 2] = 0.2f;
			}
			for (Area::id a = 1; a <= AreasCount(); ++a) {
				for (Area::id b = 1; b < a; ++b) {
					const auto& chokes = m_ChokePointsMatrix[a][b];
					for (const ChokePoint& cp : chokes) {
						WalkPosition p = cp.Center();
						int i = p.x + sectorXSize * p.y;
						win.second[i * 3 + 0] = 0.2f;
						win.second[i * 3 + 1] = 0.2f;
						win.second[i * 3 + 2] = 0.9f;
//						const std::deque<WalkPosition>& g = cp.Geometry();
//						for (WalkPosition gp : g) {
//							int i = gp.x + sectorXSize * gp.y;
//							win.second[i * 3 + 0] = 0.9f;
//							win.second[i * 3 + 1] = 0.9f;
//							win.second[i * 3 + 2] = 0.9f;
//						}
					}
				}
			}
			debugDrawer->DrawTex(win.first, win.second);
			break;
		}
	}

//	for (const STerrainMapImmobileType& mt : areaData.immobileType) {
//		std::pair<Uint32, float*> win = sdlWindows[winNum++];
//		for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
//			if (mt.sector.find(i) != mt.sector.end()) LAND(win.second, i)
//			else if (sector[i].maxElevation < 0.0) WATER(win.second, i)
//			else if (sector[i].maxSlope > 0.5) HILL(win.second, i)
//			else BLOCK(win.second, i)
//		}
//		debugDrawer->DrawTex(win.first, win.second);
//	}
}

void CTerrainData::ToggleVis(int frame)
{
	if ((debugDrawer == nullptr) || (toggleFrame >= frame)) {
		return;
	}
	toggleFrame = frame;

	if (sdlWindows.empty()) {
		// ~area
		SAreaData& areaData = *GetNextAreaData();

		std::pair<Uint32, float*> win;
		win.second = new float [sectorXSize * sectorZSize * 3];
		win.first = debugDrawer->AddSDLWindow(sectorXSize, sectorZSize, "Circuit AI :: Terrain");
		sdlWindows.push_back(win);

//		for (const STerrainMapMobileType& mt : areaData.mobileType) {
//			std::pair<Uint32, float*> win;
//			win.second = new float [sectorXSize * sectorZSize * 3];
//			std::ostringstream label;
//			label << "Circuit AI :: Terrain :: Mobile [" << mt.moveData->GetName() << "] h=" << mt.canHover << " f=" << mt.canFloat << " mb=" << mt.area.size();
//			win.first = debugDrawer->AddSDLWindow(sectorXSize, sectorZSize, label.str().c_str());
//			sdlWindows.push_back(win);
//		}
		int id = 0;
		for (const STerrainMapMobileType& mt : areaData.mobileType) {
			if (id++ == drawMTID) {
				std::pair<Uint32, float*> win;
				win.second = new float [sectorXSize * sectorZSize * 3];
				std::ostringstream label;
				label << "Circuit AI :: Terrain :: Mobile [" << mt.moveData->GetName() << "] h=" << mt.canHover << " f=" << mt.canFloat << " mb=" << mt.area.size();
				win.first = debugDrawer->AddSDLWindow(sectorXSize, sectorZSize, label.str().c_str());
				sdlWindows.push_back(win);
			}
		}

//		int itId = 0;
//		for (const STerrainMapImmobileType& mt : areaData.immobileType) {
//			std::pair<Uint32, float*> win;
//			win.second = new float [sectorXSize * sectorZSize * 3];
//			std::ostringstream label;
//			label << "Circuit AI :: Terrain :: Immobile [" << itId++ << "] h=" << mt.canHover << " f=" << mt.canFloat << " mb=" << mt.sector.size();
//			win.first = debugDrawer->AddSDLWindow(sectorXSize, sectorZSize, label.str().c_str());
//			sdlWindows.push_back(win);
//		}

		UpdateVis();
	} else {
		for (const std::pair<Uint32, float*>& win : sdlWindows) {
			debugDrawer->DelSDLWindow(win.first);
			delete[] win.second;
		}
		sdlWindows.clear();
	}
}
#endif

} // namespace circuit
