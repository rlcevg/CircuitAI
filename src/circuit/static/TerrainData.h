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

#include "AIFloat3.h"

#include <map>
#include <list>
#include <vector>
#include <atomic>

namespace springai {
	class MoveData;
}

namespace circuit {

class CCircuitAI;

struct STerrainMapArea;
struct STerrainMapAreaSector;
struct STerrainMapMobileType;
struct STerrainMapImmobileType;
struct STerrainMapSector;

struct STerrainMapAreaSector {
	STerrainMapAreaSector() :
		S(nullptr),
		area(nullptr),
		areaClosest(nullptr)
	{};

	// NOTE: some of these values are loaded as they become needed, use GlobalTerrainMap functions
	STerrainMapSector* S;  // always valid
	STerrainMapArea* area;  // The TerrainMapArea this sector belongs to, otherwise = 0 until
	STerrainMapArea* areaClosest;  // uninitialized, = the TerrainMapArea closest to this sector
	// Use this to find the closest sector useable by a unit with a different MoveType, the 0 pointer may be valid as a key index
	std::map<STerrainMapMobileType*, STerrainMapAreaSector*> sectorAlternativeM;  // uninitialized
	std::map<STerrainMapImmobileType*, STerrainMapSector*> sectorAlternativeI;  // uninitialized
};

struct STerrainMapArea {
	STerrainMapArea(int areaIUSize, STerrainMapMobileType* TMMobileType) :
		index(areaIUSize),
		mobileType(TMMobileType),
		percentOfMap(.0f),
		areaUsable(false)
	{};

	bool areaUsable;  // Should units of this type be used in this area
	int index;
	STerrainMapMobileType* mobileType;
	std::map<int, STerrainMapAreaSector*> sector;         // key = sector index, a list of all sectors belonging to it
	std::map<int, STerrainMapAreaSector*> sectorClosest;  // key = sector indexes not in "sector", indicates the sector belonging to this map-area with the closest distance
	// NOTE: use TerrainData::GetClosestSector: these values are not initialized but are instead loaded as they become needed
	float percentOfMap;  // 0-100
};

#define MAP_AREA_LIST_SIZE 50

struct STerrainMapMobileType {
	STerrainMapMobileType() :
		typeUsable(false),
		areaLargest(nullptr),
		udCount(0),
		canFloat(false),
		canHover(false),
		minElevation(.0f),
		maxElevation(.0f),
		maxSlope(.0f),
		moveData(nullptr)
	{};

	~STerrainMapMobileType();

	bool typeUsable;  // Should units of this type be used on this map
	std::vector<STerrainMapAreaSector> sector;  // Each MoveType has it's own sector list, GlobalTerrainMap->GetSectorIndex() gives an index
	std::vector<STerrainMapArea*> area;  // Each MoveType has it's own MapArea list
	STerrainMapArea* areaLargest;  // Largest area usable by this type, otherwise = 0

	float maxSlope;      // = MoveData*->maxSlope
	float maxElevation;  // = -ud->minWaterDepth
	float minElevation;  // = -MoveData*->depth
	bool canHover;
	bool canFloat;
	springai::MoveData* moveData;  // owner
	int udCount;
};

struct STerrainMapSector
{
	STerrainMapSector() :
		percentLand(.0f),
		maxSlope(.0f),
		maxElevation(.0f),
		minElevation(.0f),
		isWater(false)
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
	STerrainMapImmobileType() :
		udCount(0),
		canFloat(false),
		canHover(false),
		minElevation(.0f),
		maxElevation(.0f),
		typeUsable(false)
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

class CTerrainData {
public:
	CTerrainData();
	virtual ~CTerrainData();
	void Init(CCircuitAI* circuit);

// ---- RAI's GlobalTerrainMap ---- BEGIN
	bool CanMoveToPos(STerrainMapArea* area, const springai::AIFloat3& destination);
	std::vector<STerrainMapAreaSector>& GetSectorList(STerrainMapArea* sourceArea = nullptr);
	STerrainMapAreaSector* GetClosestSector(STerrainMapArea* sourceArea, const int& destinationSIndex);
	STerrainMapSector* GetClosestSector(STerrainMapImmobileType* sourceIT, const int& destinationSIndex);
	STerrainMapAreaSector* GetAlternativeSector(STerrainMapArea* sourceArea, const int& sourceSIndex, STerrainMapMobileType* destinationMT);
	STerrainMapSector* GetAlternativeSector(STerrainMapArea* destinationArea, const int& sourceSIndex, STerrainMapImmobileType* destinationIT); // can return 0
	int GetSectorIndex(const springai::AIFloat3& position); // use IsSectorValid() to insure the index is valid
	bool IsSectorValid(const int& sIndex);

	std::list<STerrainMapMobileType> mobileType;             // Used for mobile units, not all movedatas are used
	std::map<int, STerrainMapMobileType*> udMobileType;      // key = ud->id, Used to find a TerrainMapMobileType for a unit
	std::list<STerrainMapImmobileType> immobileType;         // Used for immobile units
	std::map<int, STerrainMapImmobileType*> udImmobileType;  // key = ud->id, Used to find a TerrainMapImmobileType for a unit
	std::vector<STerrainMapAreaSector> sectorAirType;        // used for flying units, GetSectorIndex gives an index
	std::vector<STerrainMapSector> sector;  // global sector data, GetSectorIndex gives an index
	STerrainMapImmobileType* landSectorType;   // 0 to the sky
	STerrainMapImmobileType* waterSectorType;  // minElevation to 0

	bool waterIsHarmful;  // Units are damaged by it (Lava/Acid map)
	bool waterIsAVoid;    // (Space map)
	float minElevation;   // 0 or less (used by cRAIUnitDefHandler, builder start selecter)
	float percentLand;    // 0 to 100 (used by cRAIUnitDefHandler)

	int sectorXSize;
	int sectorZSize;
	int convertStoP;  // Sector to Position: times this value for convertion, divide for the reverse

private:
	int GetFileValue(int& fileSize, char*& file, std::string entry);
// ---- RAI's GlobalTerrainMap ---- END

// ---- UNUSED so far ---- BEGIN
public:
	bool IsInitialized();
	bool IsClusterizing();
	void SetClusterizing(bool value);

	const std::vector<springai::AIFloat3>& GetDefencePoints() const;
	const std::vector<springai::AIFloat3>& GetDefencePerimeter() const;

	void Clusterize(const std::vector<springai::AIFloat3>& wayPoints, float maxDistance, CCircuitAI* circuit);

	// debug, could be used for defence perimeter calculation
//	void DrawConvexHulls(springai::Drawer* drawer);
//	void DrawCentroids(springai::Drawer* drawer);
//	void ClearMetalClusters(springai::Drawer* drawer);

private:
	bool initialized;
	std::vector<springai::AIFloat3> points;

	std::atomic<bool> isClusterizing;
// ---- UNUSED so far ---- END
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_TERRAINDATA_H_