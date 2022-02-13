/*
 * TerrainData.cpp
 *
 *  Created on: Dec 15, 2014
 *      Author: rlcevg
 */

#include "terrain/TerrainData.h"
#include "terrain/TerrainManager.h"
#include "scheduler/Scheduler.h"
#include "setup/SetupManager.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/math/HierarchCluster.h"
#include "util/math/RagMatrix.h"
#include "util/Utils.h"
#include "json/json.h"

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

namespace terrain {

using namespace circuit;
using namespace springai;

#define AREA_UPDATE_RATE	(FRAMES_PER_SEC * 10)
// FIXME: Make Engine consts available to AI. @see rts/Sim/MoveTypes/MoveDefHandler.cpp
#define MAX_ALLOWED_WATER_DAMAGE_GMM	1e3f
#define MAX_ALLOWED_WATER_DAMAGE_HMM	1e4f

float CTerrainData::boundX(0.f);
float CTerrainData::boundZ(0.f);
int CTerrainData::convertStoP(1);
CMap* CTerrainData::map(nullptr);

CTerrainData::CTerrainData()
		: pAreaData(&areaData0)
		, waterIsHarmful(false)
		, waterIsAVoid(false)
		, gameAttribute(nullptr)
		, slopeMapXSize(0)
		, isUpdating(false)
		, aiToUpdate(0)
		, isInitialized(false)
		, isAnalyzed(false)
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
	}
#endif
}

void CTerrainData::Init(CCircuitAI* circuit)
{
	if (isInitialized) {
		return;
	}

	map = circuit->GetMap();
	scheduler = circuit->GetScheduler();
	gameAttribute = circuit->GetGameAttribute();
	circuit->LOG("Loading the Terrain-Map ...");

	/*
	 *  Assign areaData references
	 */
	SAreaData& areaData = *pAreaData.load();
	std::vector<SMobileType>& mobileType = areaData.mobileType;
	std::vector<SImmobileType>& immobileType = areaData.immobileType;
	std::vector<SAreaSector>& sectorAirType = areaData.sectorAirType;
	std::vector<SSector>& sector = areaData.sector;
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
	areaData.heightMapXSize = mapWidth;
	convertStoP = DEFAULT_SLACK;  // = 2^x, should not be less than 16 (2*SUQARE_SIZE)
	constexpr int SMALL_MAP = 8;
	constexpr int LARGE_MAP = 16;
	if ((mapWidth / 64) * (mapHeight / 64) < SMALL_MAP * SMALL_MAP) {
		convertStoP /= 2; // Smaller Sectors, more detailed analysis
	} else if ((mapWidth / 64) * (mapHeight / 64) > LARGE_MAP * LARGE_MAP) {
		convertStoP *= 2; // Larger Sectors, less detailed analysis
	}
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
			SMobileType* MT = nullptr;
			int mtIdx = 0;
			for (; (unsigned)mtIdx < mobileType.size(); ++mtIdx) {
				SMobileType& mt = mobileType[mtIdx];
				if (((mt.maxElevation == -minWaterDepth) && (mt.maxSlope == maxSlope) && (mt.canHover == canHover) && (mt.canFloat == canFloat)) &&
					((mt.minElevation == -depth) || ((mt.canHover || mt.canFloat) && (mt.minElevation <= 0) && (-maxWaterDepth <= 0))))
				{
					MT = &mt;
					break;
				}
			}
			if (MT == nullptr) {
				SMobileType MT2;
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
			SImmobileType* IT = nullptr;
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
				SImmobileType IT2;
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
	const int convertStoSM = GetConvertStoSM();  // * for conversion, / for reverse conversion
	const int convertStoHM = GetConvertStoHM();  // * for conversion, / for reverse conversion
	slopeMapXSize = sectorXSize * convertStoSM;
	const int heightMapXSize = areaData.heightMapXSize;  // sectorXSize * convertStoHM;

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
		it.typeUsable = (((100.0 * it.sector.size()) / float(sectorXSize * sectorZSize) >= 16.0) || ((double)convertStoP * convertStoP * it.sector.size() >= 1.8e7));
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
	nextAreaData.heightMapXSize = mapWidth;
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

void CTerrainData::AnalyzeMap(CCircuitAI* circuit)
{
	if (isAnalyzed) {
		return;
	}

	CTerrainAnalyzer::SConfig cfg = ReadConfig(circuit);

	const SMobileType* mobileType = nullptr;
	CCircuitDef* cdef = circuit->GetCircuitDef(cfg.unitName.c_str());
	if (cdef == nullptr) {
		for (const SMobileType& mt : areaData0.mobileType) {
			if (mt.typeUsable) {
				mobileType = &mt;
				break;
			}
		}
	} else {
		mobileType = &areaData0.mobileType[cdef->GetMobileId()];
	}

	sectors.resize(sectorXSize * sectorZSize);
	if (mobileType != nullptr) {
		const int convertStoSM = GetConvertStoSM();  // * for conversion, / for reverse conversion
		cfg.tileSize = int2(sectorXSize * convertStoSM, sectorZSize * convertStoSM);
		CTerrainAnalyzer(this, cfg).AnalyzeMap(circuit, *mobileType);
	}

	isAnalyzed = true;
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

CTerrainAnalyzer::SConfig CTerrainData::ReadConfig(CCircuitAI* circuit)
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& terrain = root["terrain"];
	CTerrainAnalyzer::SConfig outCfg;
	outCfg.unitName = terrain.get("analyze", "").asString();
	outCfg.lakeMaxTiles = terrain.get("lake_tiles", 300).asInt();
	outCfg.lakeMaxWidthInTiles = terrain.get("lake_width_tiles", 32).asInt();
	outCfg.areaMinTiles = terrain.get("area_min_tiles", 64).asInt();
	outCfg.areasMergeSize = terrain.get("areas_merge_size", 80).asInt();
	outCfg.areasMergeAltitude = terrain.get("areas_merge_altitude", 80).asInt();
	return outCfg;
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
	std::vector<SMobileType>& mobileType = areaData.mobileType;
	std::vector<SImmobileType>& immobileType = areaData.immobileType;
	std::vector<SSector>& sector = areaData.sector;
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
	const int convertStoSM = GetConvertStoSM();  // * for conversion, / for reverse conversion
	const int convertStoHM = GetConvertStoHM();  // * for conversion, / for reverse conversion
	const int heightMapXSize = areaData.heightMapXSize;

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
		it.typeUsable = (((100.0 * it.sector.size()) / float(sectorXSize * sectorZSize) >= 16.0) || ((double)convertStoP * convertStoP * it.sector.size() >= 1.8e7));
	}

	/*
	 *  Determine areas per mobileType
	 */
	auto shouldRebuild = [this, &changedSectors, &sector](SMobileType& mt) {
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
				std::map<int, SAreaSector*>& sector = mt.area.back().sector;
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
	UpdateTAVis();
#endif
}

bool CTerrainData::IsWalkable(int xSlope, int ySlope, const SMobileType& mt) const
{
	const float elevation = areaData0.heightMap[areaData0.heightMapXSize * (ySlope * 2) + (xSlope * 2)];
	const float slope = slopeMap[slopeMapXSize * ySlope + xSlope];
	return (mt.canHover && (mt.maxElevation >= elevation) && !waterIsAVoid && ((elevation <= 0) || (mt.maxSlope >= slope)))
		|| (mt.canFloat && (mt.maxElevation >= elevation) && !waterIsHarmful && ((elevation <= 0) || (mt.maxSlope >= slope)))
		|| ((mt.maxSlope >= slope) && (mt.minElevation <= elevation) && (mt.maxElevation >= elevation) && (!waterIsHarmful || (elevation >= 0)));
}

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
	std::vector<SSector>& sector = areaData.sector;
	int winNum = 0;

	std::pair<Uint32, float*> win = sdlWindows[winNum++];
	for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
		if (sector[i].maxElevation < 0.0) WATER(win.second, i)
		else if (sector[i].maxSlope > 0.5) MOUNTAIN(win.second, i)
		else if (sector[i].maxSlope > 0.25) HILL(win.second, i)
		else LAND(win.second, i)
	}
	debugDrawer->DrawTex(win.first, win.second);

	for (const SMobileType& mt : areaData.mobileType) {
		std::pair<Uint32, float*> win = sdlWindows[winNum++];
		for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
			if (mt.sector[i].area != nullptr) LAND(win.second, i)
			else if (sector[i].maxElevation < 0.0) WATER(win.second, i)
			else if (sector[i].maxSlope > 0.5) HILL(win.second, i)
			else BLOCK(win.second, i)
		}
		debugDrawer->DrawTex(win.first, win.second);
	}

	for (const SImmobileType& mt : areaData.immobileType) {
		std::pair<Uint32, float*> win = sdlWindows[winNum++];
		for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
			if (mt.sector.find(i) != mt.sector.end()) LAND(win.second, i)
			else if (sector[i].maxElevation < 0.0) WATER(win.second, i)
			else if (sector[i].maxSlope > 0.5) HILL(win.second, i)
			else BLOCK(win.second, i)
		}
		debugDrawer->DrawTex(win.first, win.second);
	}
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

		for (const SMobileType& mt : areaData.mobileType) {
			std::pair<Uint32, float*> win;
			win.second = new float [sectorXSize * sectorZSize * 3];
			std::ostringstream label;
			label << "Circuit AI :: Terrain :: Mobile [" << mt.moveData->GetName() << "] h=" << mt.canHover << " f=" << mt.canFloat << " mb=" << mt.area.size();
			win.first = debugDrawer->AddSDLWindow(sectorXSize, sectorZSize, label.str().c_str());
			sdlWindows.push_back(win);
		}

		int itId = 0;
		for (const SImmobileType& mt : areaData.immobileType) {
			std::pair<Uint32, float*> win;
			win.second = new float [sectorXSize * sectorZSize * 3];
			std::ostringstream label;
			label << "Circuit AI :: Terrain :: Immobile [" << itId++ << "] h=" << mt.canHover << " f=" << mt.canFloat << " mb=" << mt.sector.size();
			win.first = debugDrawer->AddSDLWindow(sectorXSize, sectorZSize, label.str().c_str());
			sdlWindows.push_back(win);
		}

		UpdateVis();
	} else {
		for (const std::pair<Uint32, float*>& win : sdlWindows) {
			debugDrawer->DelSDLWindow(win.first);
			delete[] win.second;
		}
		sdlWindows.clear();
	}
}
#endif  // DEBUG_VIS

} // namespace terrain
