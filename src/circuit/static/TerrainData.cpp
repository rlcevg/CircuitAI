/*
 * TerrainData.cpp
 *
 *  Created on: Dec 15, 2014
 *      Author: rlcevg
 */

#include "static/TerrainData.h"
#include "static/GameAttribute.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/RagMatrix.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Map.h"
//#include "File.h"
#include "UnitDef.h"
#include "MoveData.h"

#include <functional>
#include <algorithm>
#include <deque>
#include <set>
#include <sstream>

namespace circuit {

using namespace springai;

// FIXME: Make Engine consts available to AI. @see rts/Sim/MoveTypes/MoveDefHandler.cpp
#define MAX_ALLOWED_WATER_DAMAGE_GMM	1e3f
#define MAX_ALLOWED_WATER_DAMAGE_HMM	1e4f

STerrainMapMobileType::~STerrainMapMobileType()
{
	utils::free_clear(area);
}

CTerrainData::CTerrainData() :
		landSectorType(nullptr),
		waterSectorType(nullptr),
		waterIsHarmful(false),
		waterIsAVoid(false),
		sectorXSize(0),
		sectorZSize(0),
		convertStoP(1),
		map(nullptr),
		gameAttribute(nullptr),
		pAreaData(&areaData0),
		pHeightMap(&heightMap0),
		updatingAreas(false),
		aiToUpdate(0),
		isClusterizing(false),
		initialized(false)
{
}

CTerrainData::~CTerrainData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete map;
}

void CTerrainData::Init(CCircuitAI* circuit)
{
	map = circuit->GetCallback()->GetMap();  // Must be ai-independent
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
	float& percentLand = areaData.percentLand;

	/*
	 *  Reading the WaterDamage and establishing sector size
	 */
	waterIsHarmful = false;
	waterIsAVoid = false;

	float waterDamage = map->GetWaterDamage();  // scaled by (UNIT_SLOWUPDATE_RATE / GAME_SPEED)
	std::string waterText = "  Water Damage: " + utils::float_to_string(waterDamage, "%-.*G");
	// @see rts/Sim/MoveTypes/MoveDefHandler.cpp
	if (waterDamage > 0) {  // >= MAX_ALLOWED_WATER_DAMAGE_GMM
		waterIsHarmful = true;
		waterText += " (This map's water is harmful to land units";
		if (waterDamage >= MAX_ALLOWED_WATER_DAMAGE_HMM) {
			waterIsAVoid = true;
			waterText += " as well as hovercraft";
		}
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
	convertStoP = 64;  // = 2^x, should not be less than 16
	if ((mapWidth / 64) * (mapHeight / 64) < 8 * 8) {
		convertStoP /= 2; // Smaller Sectors, more detailed analysis
//	} else if ((mapWidth / 16) * (mapHeight / 16) > 20 * 20 ) {
//		convertStoP *= 2; // Larger Sectors, less detailed analysis
	}
	sectorXSize = (SQUARE_SIZE * mapWidth) / convertStoP;
	sectorZSize = (SQUARE_SIZE * mapHeight) / convertStoP;

	sectorAirType.resize(sectorXSize * sectorZSize);

	circuit->LOG("  Sector-Map Block Size: %i", convertStoP);
	circuit->LOG("  Sector-Map Size: %li (x%i, z%i)", sectorXSize * sectorZSize, sectorXSize, sectorZSize);

	/*
	 *  MoveType Detection and TerrainMapMobileType Initialization
	 */
	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
	for (auto& kv : defs) {
		UnitDef* def = kv.second;

		if (def->IsAbleToFly()) {

			udMobileType[def->GetUnitDefId()] = -1;

		} else if (def->GetSpeed() <= 0) {

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

		} else {

			float minWaterDepth = def->GetMinWaterDepth();
			float maxWaterDepth = def->GetMaxWaterDepth();
			bool canHover = def->IsAbleToHover();
			bool canFloat = def->IsFloater();
			std::shared_ptr<MoveData> moveData(def->GetMoveData());
			float maxSlope = moveData->GetMaxSlope();
			float depth = moveData->GetDepth();
			STerrainMapMobileType* MT = nullptr;
			int mtIdx = 0;
			for (; mtIdx < mobileType.size(); ++mtIdx) {
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
		}
	}

	/*
	 *  Special types
	 */
	landSectorType = nullptr;
	waterSectorType = nullptr;
	for (auto& it : immobileType) {
		if (!it.canFloat && !it.canHover) {
			if ((it.minElevation == 0) && ((landSectorType == nullptr) || (it.maxElevation > landSectorType->maxElevation))) {
				landSectorType = &it;
			}
			if ((it.maxElevation == 0) && ((waterSectorType == nullptr) || (it.minElevation < waterSectorType->minElevation))) {
				waterSectorType = &it;
			}
		}
	}
	if (landSectorType == nullptr) {
		immobileType.push_back(STerrainMapImmobileType());
		landSectorType = &immobileType.back();
		immobileType.back().maxElevation = 1e7;
		immobileType.back().minElevation = 0;
		immobileType.back().canFloat = false;
		immobileType.back().canHover = false;
	}
	if (waterSectorType == nullptr) {
		immobileType.push_back(STerrainMapImmobileType());
		waterSectorType = &immobileType.back();
		immobileType.back().maxElevation = 0;
		immobileType.back().minElevation = -1e7;
		immobileType.back().canFloat = false;
		immobileType.back().canHover = false;
	}

	circuit->LOG("  Determining Usable Terrain for all units ...");
	/*
	 *  Setting sector & determining sectors for immobileType
	 */
	sector.resize(sectorXSize * sectorZSize);
	const std::vector<float>& standardSlopeMap = circuit->GetMap()->GetSlopeMap();
	const std::vector<float>& standardHeightMap = *pHeightMap.load() = circuit->GetMap()->GetHeightMap();
	const int convertStoSM = convertStoP / 16;  // * for conversion, / for reverse conversion
	const int convertStoHM = convertStoP / 8;  // * for conversion, / for reverse conversion
	const int slopeMapXSize = sectorXSize * convertStoSM;
	const int heightMapXSize = sectorXSize * convertStoHM;

	minElevation = 0;
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
						if (minElevation > standardHeightMap[i]) {
							minElevation = standardHeightMap[i];
						}
					} else if (sector[i].maxElevation < standardHeightMap[iH]) {
						sector[i].maxElevation = standardHeightMap[iH];
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

	for (auto& it : immobileType) {
		std::string itText = "  Immobile-Type: Min/Max Elevation=(";
		if (it.canHover) {
			itText += "hover";
		} else if (it.canFloat || (it.minElevation < -10000)) {
			itText += "any";
		} else {
			itText += utils::float_to_string(it.minElevation, "%-.*G");
		}
		itText += " / ";
		if (it.maxElevation < 10000) {
			itText += utils::float_to_string(it.maxElevation, "%-.*G");
		} else {
			itText += "any";
		}
		float percentMap = (100.0 * it.sector.size()) / (sectorXSize * sectorZSize);
		itText += ")  \tIs buildable across " + utils::float_to_string(percentMap, "%-.4G") + "%% of the map. (used by %d unit-defs)";
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
				mt.area.back()->sector[i] = &mt.sector[i];
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

				if ((areaSize > 0) && ((areaSize == MAP_AREA_LIST_SIZE) || (mt.area.back()->sector.size() <= MAMinimalSectors) ||
					(100. * float(mt.area.back()->sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
				{
					// Too many areas detected. Find, erase & ignore the smallest one that was found so far
					if (areaSize == MAP_AREA_LIST_SIZE) {
						mtText << "\nWARNING: The MapArea limit has been reached (possible error).";
					}
					decltype(mt.area)::iterator it, itArea;
					it = itArea = mt.area.begin();
					for (++it; it != mt.area.end(); ++it) {
						if ((*it)->sector.size() < (*itArea)->sector.size()) {
							itArea = it;
						}
					}
					delete *itArea;
					mt.area.erase(itArea);
					areaSize--;
				}

				i = *sectorsRemaining.begin();
				sectorSearch.push_back(i);
				sectorsRemaining.erase(i);
				mt.area.push_back(new STerrainMapArea(&mt));
				areaSize++;
			}
		}
		if ((areaSize > 0) && ((mt.area.back()->sector.size() <= MAMinimalSectors) ||
			(100.0 * float(mt.area.back()->sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
		{
			areaSize--;
			delete mt.area.back();
			mt.area.pop_back();
		}

		// Calculations
		float percentOfMap = 0.0;
		for (auto area : mt.area) {
			for (auto& iS : area->sector) {
				iS.second->area = area;
			}
			area->percentOfMap = (100.0 * area->sector.size()) / (sectorXSize * sectorZSize);
			if (area->percentOfMap >= 20.0 ) {  // A map area occupying 20% of the map
				area->areaUsable = true;
				mt.typeUsable = true;
			} else {
				area->areaUsable = false;
			}
			if ((mt.areaLargest == nullptr) || (mt.areaLargest->percentOfMap < area->percentOfMap)) {
				mt.areaLargest = area;
			}

			percentOfMap += area->percentOfMap;
		}
		mtText << "  \tHas " << areaSize << " Map-Area(s) occupying " << percentOfMap << "%% of the map. (used by " << mt.udCount << " unit-defs)";
		circuit->LOG(mtText.str().c_str());
	}

	/*
	 *  Duplicate areaData
	 */
	SAreaData& nextAreaData = (pAreaData.load() == &areaData0) ? areaData1 : areaData0;
	nextAreaData.mobileType = mobileType;
	for (auto& mt : nextAreaData.mobileType) {
		std::list<STerrainMapArea*> cpAreas;
		for (auto area : mt.area) {
			STerrainMapArea* cpArea = new STerrainMapArea(&mt);
			for (auto& kv : area->sector) {
				cpArea->sector[kv.first] = &mt.sector[kv.first];
			}
			cpAreas.push_back(cpArea);
		}
		mt.area = cpAreas;
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

	scheduler->RunTaskEvery(std::make_shared<CGameTask>(&CTerrainData::CheckHeightMap, this), FRAMES_PER_SEC * 60);

	initialized = true;

//	/*
//	 *  Debugging
//	 */
//	std::ostringstream deb;
//	for (int iS = 0; iS < sectorXSize * sectorZSize; iS++) {
//		if (iS % sectorXSize == 0) deb << "\n";
//		if (sector[iS].maxElevation < 0.0) deb << "~";
//		else if (sector[iS].maxSlope > 0.5) deb << "^";
//		else if (sector[iS].maxSlope > 0.25) deb << "#";
//		else deb << "x";
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
//	deb << "\n";
//	circuit->LOG(deb.str().c_str());
}

int CTerrainData::GetSectorIndex(const AIFloat3& position)
{
	return sectorXSize * (int(position.z) / convertStoP) + int(position.x) / convertStoP;
}

bool CTerrainData::IsSectorValid(const int& sIndex)
{
	if ((sIndex < 0) || (sIndex >= sectorXSize * sectorZSize)) {
		return false;
	}
	return true;
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

void CTerrainData::CheckHeightMap()
{
	if (updatingAreas) {
		return;
	}
	updatingAreas = true;
	std::vector<float>& heightMap = (pHeightMap.load() == &heightMap0) ? heightMap1 : heightMap0;
	heightMap = map->GetHeightMap();
	slopeMap = map->GetSlopeMap();
	scheduler->RunParallelTask(std::make_shared<CGameTask>(&CTerrainData::UpdateAreas, this),
							   std::make_shared<CGameTask>(&CTerrainData::ScheduleUsersUpdate, this));
}

void CTerrainData::UpdateAreas()
{
	/*
	 *  Assign areaData references
	 */
	SAreaData& areaData = (pAreaData.load() == &areaData0) ? areaData1 : areaData0;
	std::vector<STerrainMapMobileType>& mobileType = areaData.mobileType;
	std::vector<STerrainMapImmobileType>& immobileType = areaData.immobileType;
	std::vector<STerrainMapSector>& sector = areaData.sector;
	float& minElevation = areaData.minElevation;
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
	for (auto& mt : prevAreaData.mobileType) {
		itmt->areaLargest = nullptr;
		for (auto& as : itmt->sector) {
			as.area = nullptr;
			as.sectorAlternativeM.clear();
			as.sectorAlternativeI.clear();
		}
		utils::free_clear(itmt->area);
		++itmt;
	}
	decltype(areaData.immobileType)::iterator itit = immobileType.begin();
	for (auto& it : prevAreaData.immobileType) {
		itit->sector.clear();
		for (auto& kv : it.sector) {
			itit->sector[kv.first] = &sector[kv.first];
		}
		itit->sectorClosest.clear();
		++itit;
	}
	minElevation = prevAreaData.minElevation;
	percentLand = prevAreaData.percentLand;

	/*
	 *  Updating sector & determining sectors for immobileType
	 */
	const std::vector<float>& standardSlopeMap = slopeMap;
	const std::vector<float>& standardHeightMap = (pHeightMap.load() == &heightMap0) ? heightMap1 : heightMap0;
	const std::vector<float>& prevHeightMap = *pHeightMap.load();
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
			int iMap = ((z * convertStoHM) * heightMapXSize) + x * convertStoHM;
			if (!isSectorHeightChanged(iMap)) {
				continue;
			}

			int i = (z * sectorXSize) + x;
			changedSectors.insert(i);

			sector[i].position.y = map->GetElevationAt(sector[i].position.x, sector[i].position.z);

			sector[i].maxSlope = .0f;
			iMap = ((z * convertStoSM) * slopeMapXSize) + x * convertStoSM;
			for (int zS = 0; zS < convertStoSM; zS++) {
				for (int xS = 0, iS = iMap + zS * slopeMapXSize + xS; xS < convertStoSM; xS++, iS = iMap + zS * slopeMapXSize + xS) {
					if (sector[i].maxSlope < standardSlopeMap[iS]) {
						sector[i].maxSlope = standardSlopeMap[iS];
					}
				}
			}

			float prevPercentLand = std::round(sector[i].percentLand * (convertStoHM * convertStoHM) / 100.0);
			sector[i].percentLand = .0f;
			sector[i].minElevation = .0f;
			sector[i].maxElevation = .0f;
			for (int zH = 0; zH < convertStoHM; zH++) {
				for (int xH = 0, iH = iMap + zH * heightMapXSize + xH; xH < convertStoHM; xH++, iH = iMap + zH * heightMapXSize + xH) {
					if (standardHeightMap[iH] >= 0) {
						sector[i].percentLand++;
					}

					if (sector[i].minElevation > standardHeightMap[iH]) {
						sector[i].minElevation = standardHeightMap[iH];
						if (minElevation > standardHeightMap[i]) {
							minElevation = standardHeightMap[i];
						}
					} else if (sector[i].maxElevation < standardHeightMap[iH]) {
						sector[i].maxElevation = standardHeightMap[iH];
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
					mt.area.back()->sector[i] = &mt.sector[i];
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

					if ((areaSize > 0) && ((areaSize == MAP_AREA_LIST_SIZE) || (mt.area.back()->sector.size() <= MAMinimalSectors) ||
						(100. * float(mt.area.back()->sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
					{
						decltype(mt.area)::iterator it, itArea;
						it = itArea = mt.area.begin();
						for (++it; it != mt.area.end(); ++it) {
							if ((*it)->sector.size() < (*itArea)->sector.size()) {
								itArea = it;
							}
						}
						delete *itArea;
						mt.area.erase(itArea);
						areaSize--;
					}

					i = *sectorsRemaining.begin();
					sectorSearch.push_back(i);
					sectorsRemaining.erase(i);
					mt.area.push_back(new STerrainMapArea(&mt));
					areaSize++;
				}
			}
			if ((areaSize > 0) && ((mt.area.back()->sector.size() <= MAMinimalSectors) ||
				(100.0 * float(mt.area.back()->sector.size()) / float(sectorXSize * sectorZSize) <= MAMinimalSectorPercent)))
			{
				delete mt.area.back();
				mt.area.pop_back();
			}

		} else {  // should not rebuild

			// Copy mt.area from previous areaData
			for (auto area : itmt->area) {
				STerrainMapArea* cpArea = new STerrainMapArea(&mt);
				for (auto& kv : area->sector) {
					cpArea->sector[kv.first] = &mt.sector[kv.first];
				}
				mt.area.push_back(cpArea);
			}
		}

		// Calculations
		for (auto area : mt.area) {
			for (auto& iS : area->sector) {
				iS.second->area = area;
			}
			area->percentOfMap = (100.0 * area->sector.size()) / (sectorXSize * sectorZSize);
			if (area->percentOfMap >= 20.0 ) {  // A map area occupying 20% of the map
				area->areaUsable = true;
				mt.typeUsable = true;
			} else {
				area->areaUsable = false;
			}
			if ((mt.areaLargest == nullptr) || (mt.areaLargest->percentOfMap < area->percentOfMap)) {
				mt.areaLargest = area;
			}
		}

		++itmt;
	}
}

void CTerrainData::ScheduleUsersUpdate()
{
	aiToUpdate = 0;
	for (auto circuit : gameAttribute->GetCircuits()) {
		if (circuit->IsInitialized()) {
			circuit->GetScheduler()->RunTaskAfter(std::make_shared<CGameTask>(&CTerrainManager::UpdateAreaUsers, circuit->GetTerrainManager()), ++aiToUpdate);
		}
	}
	// Check if there are any ai to update
	++aiToUpdate;
	DidUpdateAreaUsers();
}

void CTerrainData::DidUpdateAreaUsers()
{
	if (--aiToUpdate == 0) {
		pAreaData = GetNextAreaData();
		pHeightMap = (pHeightMap.load() == &heightMap0) ? &heightMap1 : &heightMap0;
		updatingAreas = false;
	}
}

SAreaData* CTerrainData::GetNextAreaData()
{
	return (pAreaData.load() == &areaData0) ? &areaData1 : &areaData0;
}

bool CTerrainData::IsInitialized()
{
	return initialized;
}

bool CTerrainData::IsClusterizing()
{
	return isClusterizing.load();
}

void CTerrainData::SetClusterizing(bool value)
{
	isClusterizing = value;
}

const std::vector<springai::AIFloat3>& CTerrainData::GetDefencePoints() const
{
	return points;
}

const std::vector<springai::AIFloat3>& CTerrainData::GetDefencePerimeter() const
{
	return points;
}

void CTerrainData::Clusterize(const std::vector<AIFloat3>& wayPoints, float maxDistance, CCircuitAI* circuit)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	int nrows = wayPoints.size();
	CRagMatrix distmatrix(nrows);
	for (int i = 1; i < nrows; i++) {
		for (int j = 0; j < i; j++) {
			float dx = wayPoints[i].x - wayPoints[j].x;
			float dz = wayPoints[i].z - wayPoints[j].z;
			distmatrix(i, j) = dx * dx + dz * dz;
		}
	}

	// Initialize cluster-element list
	std::vector<std::vector<int>> iclusters;
	iclusters.reserve(nrows);
	for (int i = 0; i < nrows; i++) {
		std::vector<int> cluster;
		cluster.push_back(i);
		iclusters.push_back(cluster);
	}

	for (int n = nrows; n > 1; n--) {
		// Find pair
		int is = 1;
		int js = 0;
		if (distmatrix.FindClosestPair(n, is, js) > maxDistance) {
			break;
		}

		// Fix the distances
		for (int j = 0; j < js; j++) {
			distmatrix(js, j) = std::max(distmatrix(is, j), distmatrix(js, j));
		}
		for (int j = js + 1; j < is; j++) {
			distmatrix(j, js) = std::max(distmatrix(is, j), distmatrix(j, js));
		}
		for (int j = is + 1; j < n; j++) {
			distmatrix(j, js) = std::max(distmatrix(j, is), distmatrix(j, js));
		}

		for (int j = 0; j < is; j++) {
			distmatrix(is, j) = distmatrix(n - 1, j);
		}
		for (int j = is + 1; j < n - 1; j++) {
			distmatrix(j, is) = distmatrix(n - 1, j);
		}

		// Merge clusters
		std::vector<int>& cluster = iclusters[js];
		cluster.reserve(cluster.size() + iclusters[is].size()); // preallocate memory
		cluster.insert(cluster.end(), iclusters[is].begin(), iclusters[is].end());
		iclusters[is] = iclusters[n - 1];
		iclusters.pop_back();
	}

	std::vector<std::vector<int>> clusters;
	std::vector<AIFloat3> centroids;
	int nclusters = iclusters.size();
	clusters.resize(nclusters);
	centroids.resize(nclusters);
	for (int i = 0; i < nclusters; i++) {
		clusters[i].clear();
		AIFloat3 centr = ZeroVector;
		for (int j = 0; j < iclusters[i].size(); j++) {
			clusters[i].push_back(iclusters[i][j]);
			centr += wayPoints[iclusters[i][j]];
		}
		centr /= iclusters[i].size();
		centroids[i] = centr;
	}

	printf("nclusters: %i\n", nclusters);
	for (int i = 0; i < clusters.size(); i++) {
		printf("%i | ", clusters[i].size());
	}
	auto compare = [](const std::vector<int>& a, const std::vector<int>& b) {
		return a.size() > b.size();
	};
	std::sort(clusters.begin(), clusters.end(), compare);
	int num = centroids.size();
	Drawer* drawer = circuit->GetMap()->GetDrawer();
	for (int i = 0; i < num; i++) {
		AIFloat3 centr = ZeroVector;
		for (int j = 0; j < clusters[i].size(); j++) {
			centr += wayPoints[clusters[i][j]];
		}
		centr /= clusters[i].size();
		drawer->AddPoint(centr, utils::string_format("%i", i).c_str());
	}
	delete drawer;

	isClusterizing = false;
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
//			// FIXME: Remove next DEBUG line
//			int M = N;
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

} // namespace circuit
