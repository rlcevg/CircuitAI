/*
 * TerrainData.h
 *
 *  Created on: Dec 15, 2014
 *      Author: rlcevg
 */
// _____________________________________________________
//
// Origin: RAI - Skirmish AI for Spring
// Author: Reth / Michael Vadovszki
// _____________________________________________________

// NOTES:
// "position blocks" refers to the in-game units of measurement
// A Map Preview Block is 512x512 position blocks
// GetMapWidth(),GetMapHeight(),GetHeightMap() uses 8x8 position blocks
// GetMetalMap(),GetSlopeMap() uses 16x16 position blocks

#ifndef SRC_CIRCUIT_TERRAIN_TERRAINDATA_H_
#define SRC_CIRCUIT_TERRAIN_TERRAINDATA_H_

#include "map/GridAnalyzer.h"
#include "util/Defines.h"

#include "AIFloat3.h"

#include <map>
#include <vector>
#include <atomic>
#include <memory>

struct SSkirmishAICallback;

namespace springai {
	class MoveData;
}

namespace circuit {
	class CCircuitAI;
	class CScheduler;
	class CGameAttribute;
	class IMainJob;
	class CMap;
}

namespace terrain {

struct SArea;
struct SAreaSector;
struct SMobileType;
struct SImmobileType;
struct SSector;

struct SAreaSector {
	SAreaSector() : S(nullptr), area(nullptr) {}

	// NOTE: some of these values are loaded as they become needed, use GlobalTerrainMap functions
	SSector* S;  // always valid
	SArea* area;  // The TerrainMapArea this sector belongs to, otherwise = 0 until
	// Use this to find the closest sector useable by a unit with a different MoveType, the 0 pointer may be valid as a key index
	std::map<SMobileType*, SAreaSector*> sectorAlternativeM;  // uninitialized
	std::map<SImmobileType*, SSector*> sectorAlternativeI;  // uninitialized
};

struct SArea {
	SArea(SMobileType* TMMobileType)
		: areaUsable(false)
		, mobileType(TMMobileType)
		, percentOfMap(.0f)
	{}

	bool areaUsable;  // Should units of this type be used in this area
	SMobileType* mobileType;
	std::map<int, SAreaSector*> sector;         // key = sector index, a list of all sectors belonging to it
	std::map<int, SAreaSector*> sectorClosest;  // key = sector indexes not in "sector", indicates the sector belonging to this map-area with the closest distance
	// NOTE: use TerrainData::GetClosestSector: these values are not initialized but are instead loaded as they become needed
	float percentOfMap;  // 0-100
};

#define MAP_AREA_LIST_SIZE	50

struct SMobileType {
	using Id = int;

	SMobileType()
		: typeUsable(false)
		, areaLargest(nullptr)
		, maxSlope(.0f)
		, maxElevation(.0f)
		, minElevation(.0f)
		, canHover(false)
		, canFloat(false)
		, moveData(nullptr)
		, udCount(0)
	{}

	bool typeUsable;  // Should units of this type be used on this map
	std::vector<SAreaSector> sector;  // Each MoveType has it's own sector list, GlobalTerrainMap->GetSectorIndex() gives an index
	std::vector<SArea> area;  // Each MoveType has it's own MapArea list
	SArea* areaLargest;  // Largest area usable by this type, otherwise = 0

	float maxSlope;      // = MoveData*->maxSlope
	float maxElevation;  // = -ud->minWaterDepth
	float minElevation;  // = -MoveData*->depth
	bool canHover;
	bool canFloat;
	std::shared_ptr<springai::MoveData> moveData;
	int udCount;
};

struct SSector {
	SSector()
		: isWater(false)
		, percentLand(.0f)
		, minElevation(.0f)
		, maxElevation(.0f)
		, maxSlope(.0f)
	{}

	bool isWater;  // (Water = true) (Land = false)
	springai::AIFloat3 position;  // center of the sector, same as unit positions

	// only used during initialization
	float percentLand;   // 0-100
	float minElevation;  // 0 or less for water
	float maxElevation;
	float maxSlope;      // 0 or higher
};

struct SImmobileType {
	using Id = int;

	SImmobileType()
		: typeUsable(false)
		, minElevation(.0f)
		, maxElevation(.0f)
		, canHover(false)
		, canFloat(false)
		, udCount(0)
	{}

	bool typeUsable;  // Should units of this type be used on this map
	std::map<int, SSector*> sector;         // a list of sectors useable by these units
	std::map<int, SSector*> sectorClosest;  // key = sector indexes not in "sector", indicates the closest sector in "sector"
	float minElevation;
	float maxElevation;
	bool canHover;
	bool canFloat;
	int udCount;
};

struct SAreaData {
	SAreaData()
		: minElevation(.0f)
		, maxElevation(.0f)
		, percentLand(.0f)
		, heightMapXSize(0)
	{}

	float GetElevationAt(float posX, float posZ) const {
		return heightMap[int(posZ) / SQUARE_SIZE * heightMapXSize + int(posX) / SQUARE_SIZE];
	}

	std::vector<SMobileType> mobileType;      // Used for mobile units, not all movedatas are used
	std::vector<SImmobileType> immobileType;  // Used for immobile units
	std::vector<SAreaSector> sectorAirType;   // used for flying units, GetSectorIndex gives an index
	std::vector<SSector> sector;  // global sector data, GetSectorIndex gives an index

	float minElevation;  // minimum elevation
	float maxElevation;  // maximum elevation
	float percentLand;  // 0 to 100

	FloatVec heightMap;
	int heightMapXSize;  // height map width
};

#define BOUND_EXT	3e3f

class CTerrainData final: public bwem::IGrid {
public:
	CTerrainData();
	virtual ~CTerrainData();
	void Init(circuit::CCircuitAI* circuit);
	void AnalyzeMap(circuit::CCircuitAI* circuit);

	static circuit::CMap* GetMap() { return map; }
	static void CorrectPosition(springai::AIFloat3& position);
	static springai::AIFloat3 CorrectPosition(const springai::AIFloat3& pos, const springai::AIFloat3& dir, float& len);

	static inline bool IsNotInBounds(const springai::AIFloat3& pos) {
		return (pos.x < -BOUND_EXT) || (pos.z < -BOUND_EXT) || (pos.x > boundX) || (pos.z > boundZ) || (pos == ZeroVector);
	}
	static float boundX;
	static float boundZ;

// >>> RAI's GlobalTerrainMap ---- BEGIN
	int GetSectorIndex(const springai::AIFloat3& position) const {  // use IsSectorValid() to insure the index is valid
		return sectorXSize * (int(position.z) / convertStoP) + int(position.x) / convertStoP;
	}
	bool IsSectorValid(const int& sIndex) {
		return (sIndex >= 0) && (sIndex < sectorXSize * sectorZSize);
	}

	SAreaData areaData0, areaData1;  // Double-buffer for threading
	std::atomic<SAreaData*> pAreaData;
	std::map<int, SMobileType::Id> udMobileType;    // key = ud->id, Used to find a TerrainMapMobileType for a unit
	std::map<int, SImmobileType::Id> udImmobileType;  // key = ud->id, Used to find a TerrainMapImmobileType for a unit

	bool waterIsHarmful;  // Units are damaged by it (Lava/Acid map)
	bool waterIsAVoid;    // (Space map)

	static int convertStoP;  // Sector to Position: times this value for conversion, divide for the reverse

private:
//	int GetFileValue(int& fileSize, char*& file, std::string entry);
// <<< RAI's GlobalTerrainMap ---- END

	bwem::CGridAnalyzer::SConfig ReadConfig(circuit::CCircuitAI* circuit);

	void DelegateAuthority(circuit::CCircuitAI* curOwner);

// >>> Threaded areas updater ---- BEGIN
private:
	void EnqueueUpdate();
	std::shared_ptr<circuit::IMainJob> UpdateAreas();
	void ScheduleUsersUpdate();
public:
	void OnAreaUsersUpdated();
	SAreaData* GetNextAreaData() {
		return (pAreaData.load() == &areaData0) ? &areaData1 : &areaData0;
	}

private:
	static circuit::CMap* map;
	std::shared_ptr<circuit::CScheduler> scheduler;
	circuit::CGameAttribute* gameAttribute;
	FloatVec slopeMap;
	int slopeMapXSize;  // slope map width
	bool isUpdating;
	int aiToUpdate;
// <<< Threaded areas updater ---- END

public:
	virtual bool IsWalkable(int xSlope, int ySlope) const;  // x, y in slope map

	int GetConvertStoSM() const { return convertStoP / SLOPE_TILE; }  // sector to slope map
	int GetConvertStoHM() const { return convertStoP / HEIGHT_TILE; }  // sector to height map

	bool IsInitialized() const { return isInitialized; }

private:
	bool isInitialized;
	bool isAnalyzed;
	SMobileType const* rmt;  // reference mobileType to analyze

#ifdef DEBUG_VIS
private:
	std::vector<std::pair<uint32_t, float*>> sdlWindows;
	void UpdateVis();
public:
	void ToggleVis(int frame);
#endif
};

} // namespace terrain

#endif // SRC_CIRCUIT_TERRAIN_TERRAINDATA_H_
