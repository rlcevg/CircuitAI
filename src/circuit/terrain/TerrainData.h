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
// A Map Preivew Block is 512x512 position blocks
// GetMapWidth(),GetMapHeight(),GetHeightMap() uses 8x8 position blocks
// GetMetalMap(),GetSlopeMap() uses 16x16 position blocks

#ifndef SRC_CIRCUIT_TERRAIN_TERRAINDATA_H_
#define SRC_CIRCUIT_TERRAIN_TERRAINDATA_H_

#include "util/Defines.h"

#include "AIFloat3.h"

#include <map>
#include <vector>
#include <atomic>
#include <memory>

struct SSkirmishAICallback;

namespace springai {
	class MoveData;
	class Map;
}

namespace circuit {

class CCircuitAI;
class CScheduler;
class CGameAttribute;
class CMap;
#ifdef DEBUG_VIS
class CDebugDrawer;
#endif

struct STerrainMapArea;
struct STerrainMapAreaSector;
struct STerrainMapMobileType;
struct STerrainMapImmobileType;
struct STerrainMapSector;

struct STerrainMapAreaSector {
	STerrainMapAreaSector() :
		S(nullptr),
		area(nullptr)
	{};

	// NOTE: some of these values are loaded as they become needed, use GlobalTerrainMap functions
	STerrainMapSector* S;  // always valid
	STerrainMapArea* area;  // The TerrainMapArea this sector belongs to, otherwise = 0 until
	// Use this to find the closest sector useable by a unit with a different MoveType, the 0 pointer may be valid as a key index
	std::map<STerrainMapMobileType*, STerrainMapAreaSector*> sectorAlternativeM;  // uninitialized
	std::map<STerrainMapImmobileType*, STerrainMapSector*> sectorAlternativeI;  // uninitialized
};

struct STerrainMapArea {
	STerrainMapArea(STerrainMapMobileType* TMMobileType) :
		areaUsable(false),
		mobileType(TMMobileType),
		percentOfMap(.0f)
	{};

	bool areaUsable;  // Should units of this type be used in this area
	STerrainMapMobileType* mobileType;
	std::map<int, STerrainMapAreaSector*> sector;         // key = sector index, a list of all sectors belonging to it
	std::map<int, STerrainMapAreaSector*> sectorClosest;  // key = sector indexes not in "sector", indicates the sector belonging to this map-area with the closest distance
	// NOTE: use TerrainData::GetClosestSector: these values are not initialized but are instead loaded as they become needed
	float percentOfMap;  // 0-100
};

#define MAP_AREA_LIST_SIZE	50

struct STerrainMapMobileType {
	using Id = int;

	STerrainMapMobileType() :
		typeUsable(false),
		areaLargest(nullptr),
		maxSlope(.0f),
		maxElevation(.0f),
		minElevation(.0f),
		canHover(false),
		canFloat(false),
		moveData(nullptr),
		udCount(0)
	{};

	bool typeUsable;  // Should units of this type be used on this map
	std::vector<STerrainMapAreaSector> sector;  // Each MoveType has it's own sector list, GlobalTerrainMap->GetSectorIndex() gives an index
	std::vector<STerrainMapArea> area;  // Each MoveType has it's own MapArea list
	STerrainMapArea* areaLargest;  // Largest area usable by this type, otherwise = 0

	float maxSlope;      // = MoveData*->maxSlope
	float maxElevation;  // = -ud->minWaterDepth
	float minElevation;  // = -MoveData*->depth
	bool canHover;
	bool canFloat;
	std::shared_ptr<springai::MoveData> moveData;
	int udCount;
};

struct STerrainMapSector {
	STerrainMapSector() :
		isWater(false),
		percentLand(.0f),
		minElevation(.0f),
		maxElevation(.0f),
		maxSlope(.0f)
	{};

	bool isWater;  // (Water = true) (Land = false)
	springai::AIFloat3 position;  // center of the sector, same as unit positions

	// only used during initialization
	float percentLand;   // 0-100
	float minElevation;  // 0 or less for water
	float maxElevation;
	float maxSlope;      // 0 or higher
};

struct STerrainMapImmobileType {
	using Id = int;

	STerrainMapImmobileType() :
		typeUsable(false),
		minElevation(.0f),
		maxElevation(.0f),
		canHover(false),
		canFloat(false),
		udCount(0)
	{};

	bool typeUsable;  // Should units of this type be used on this map
	std::map<int, STerrainMapSector*> sector;         // a list of sectors useable by these units
	std::map<int, STerrainMapSector*> sectorClosest;  // key = sector indexes not in "sector", indicates the closest sector in "sector"
	float minElevation;
	float maxElevation;
	bool canHover;
	bool canFloat;
	int udCount;
};

struct SAreaData {
	SAreaData() :
		minElevation(.0f),
		maxElevation(.0f),
		percentLand(.0f)
	{};

	std::vector<STerrainMapMobileType> mobileType;      // Used for mobile units, not all movedatas are used
	std::vector<STerrainMapImmobileType> immobileType;  // Used for immobile units
	std::vector<STerrainMapAreaSector> sectorAirType;   // used for flying units, GetSectorIndex gives an index
	std::vector<STerrainMapSector> sector;  // global sector data, GetSectorIndex gives an index

	float minElevation;  // minimum elevation
	float maxElevation;  // maximum elevation
	float percentLand;  // 0 to 100

	FloatVec heightMap;
};

#define BOUND_EXT	3e3f

class CTerrainData {
public:
	CTerrainData();
	virtual ~CTerrainData();
	void Init(CCircuitAI* circuit);

	static CMap* GetMap() { return map; }
	static void CorrectPosition(springai::AIFloat3& position);

	static inline bool IsNotInBounds(const springai::AIFloat3& pos) {
		return (pos.x < -BOUND_EXT) || (pos.z < -BOUND_EXT) || (pos.x > boundX) || (pos.z > boundZ) || (pos == ZeroVector);
	}
	static float boundX;
	static float boundZ;

// ---- RAI's GlobalTerrainMap ---- BEGIN
	int GetSectorIndex(const springai::AIFloat3& position) const {  // use IsSectorValid() to insure the index is valid
		return sectorXSize * (int(position.z) / convertStoP) + int(position.x) / convertStoP;
	}
	bool IsSectorValid(const int& sIndex) {
		return (sIndex >= 0) && (sIndex < sectorXSize * sectorZSize);
	}

	SAreaData areaData0, areaData1;  // Double-buffer for threading
	std::atomic<SAreaData*> pAreaData;
	std::map<int, STerrainMapMobileType::Id> udMobileType;    // key = ud->id, Used to find a TerrainMapMobileType for a unit
	std::map<int, STerrainMapImmobileType::Id> udImmobileType;  // key = ud->id, Used to find a TerrainMapImmobileType for a unit

	bool waterIsHarmful;  // Units are damaged by it (Lava/Acid map)
	bool waterIsAVoid;    // (Space map)

	int sectorXSize;
	int sectorZSize;
	static int convertStoP;  // Sector to Position: times this value for convertion, divide for the reverse

private:
//	int GetFileValue(int& fileSize, char*& file, std::string entry);
// ---- RAI's GlobalTerrainMap ---- END

	void DelegateAuthority(CCircuitAI* curOwner);

// ---- Threaded areas updater ---- BEGIN
private:
	void EnqueueUpdate();
	void UpdateAreas();
	void ScheduleUsersUpdate();
public:
	void OnAreaUsersUpdated();
	SAreaData* GetNextAreaData() {
		return (pAreaData.load() == &areaData0) ? &areaData1 : &areaData0;
	}

private:
	static CMap* map;
	std::shared_ptr<CScheduler> scheduler;
	CGameAttribute* gameAttribute;
	FloatVec slopeMap;
	bool isUpdating;
	int aiToUpdate;
// ---- Threaded areas updater ---- END

public:
	bool IsInitialized() const { return isInitialized; }

	// debug, could be used for defence perimeter calculation
//	void DrawConvexHulls(springai::Drawer* drawer);
//	void DrawCentroids(springai::Drawer* drawer);
//	void ClearMetalClusters(springai::Drawer* drawer);

private:
	bool isInitialized;

#ifdef DEBUG_VIS
private:
	std::shared_ptr<CDebugDrawer> debugDrawer;
	std::vector<std::pair<uint32_t, float*>> sdlWindows;
	int toggleFrame;
	void UpdateVis();
public:
	void ToggleVis(int frame);
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_TERRAINDATA_H_
