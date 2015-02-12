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

struct TerrainMapArea;
struct TerrainMapAreaSector;
struct TerrainMapMobileType;
struct TerrainMapImmobileType;
struct TerrainMapSector;

struct TerrainMapAreaSector {
	TerrainMapAreaSector() :
		S(nullptr),
		area(nullptr),
		areaClosest(nullptr)
	{};

	// NOTE: some of these values are loaded as they become needed, use GlobalTerrainMap functions
	TerrainMapSector* S;  // always valid
	TerrainMapArea* area;  // The TerrainMapArea this sector belongs to, otherwise = 0 until
	TerrainMapArea* areaClosest;  // uninitialized, = the TerrainMapArea closest to this sector
	// Use this to find the closest sector useable by a unit with a different MoveType, the 0 pointer may be valid as a key index
	std::map<TerrainMapMobileType*, TerrainMapAreaSector*> sectorAlternativeM;  // uninitialized
	std::map<TerrainMapImmobileType*, TerrainMapSector*> sectorAlternativeI;  // uninitialized
};

struct TerrainMapArea {
	TerrainMapArea(int areaIUSize, TerrainMapMobileType* TMMobileType) :
		index(areaIUSize),
		mobileType(TMMobileType),
		percentOfMap(.0f),
		areaUsable(false)
	{};

	bool areaUsable;  // Should units of this type be used in this area
	int index;
	TerrainMapMobileType* mobileType;
	std::map<int, TerrainMapAreaSector*> sector;         // key = sector index, a list of all sectors belonging to it
	std::map<int, TerrainMapAreaSector*> sectorClosest;  // key = sector indexes not in "sector", indicates the sector belonging to this map-area with the closest distance
	// NOTE: use TerrainData::GetClosestSector: these values are not initialized but are instead loaded as they become needed
	float percentOfMap;  // 0-100
};

#define MAP_AREA_LIST_SIZE 50

struct TerrainMapMobileType {
	TerrainMapMobileType() :
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

	~TerrainMapMobileType();

	bool typeUsable;  // Should units of this type be used on this map
	std::vector<TerrainMapAreaSector> sector;  // Each MoveType has it's own sector list, GlobalTerrainMap->GetSectorIndex() gives an index
	std::vector<TerrainMapArea*> area;  // Each MoveType has it's own MapArea list
	TerrainMapArea* areaLargest;  // Largest area usable by this type, otherwise = 0

	float maxSlope;		 // = MoveData*->maxSlope
	float maxElevation;  // = -ud->minWaterDepth
	float minElevation;  // = -MoveData*->depth
	bool canHover;
	bool canFloat;
	springai::MoveData* moveData;  // owner
	int udCount;
};

struct TerrainMapSector
{
	TerrainMapSector() :
		percentLand(.0f),
		maxSlope(.0f),
		maxElevation(.0f),
		minElevation(.0f),
		isWater(false)
	{};

	bool isWater;		// (Water = true) (Land = false)
	float3 position;	// center of the sector, same as unit positions

	// only used during initialization
	float percentLand;   // 0-100
	float minElevation;  // 0 or less for water
	float maxElevation;
	float maxSlope;      // 0 or higher
};

struct TerrainMapImmobileType {
	TerrainMapImmobileType() :
		udCount(0),
		canFloat(false),
		canHover(false),
		minElevation(.0f),
		maxElevation(.0f),
		typeUsable(false)
	{};

	bool typeUsable;  // Should units of this type be used on this map
	std::map<int, TerrainMapSector*> sector;         // a list of sectors useable by these units
	std::map<int, TerrainMapSector*> sectorClosest;  // key = sector indexes not in "sector", indicates the closest sector in "sector"
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
	bool CanMoveToPos(TerrainMapArea* area, const float3& destination);
	std::vector<TerrainMapAreaSector>& GetSectorList(TerrainMapArea* sourceArea = nullptr);
	TerrainMapAreaSector* GetClosestSector(TerrainMapArea* sourceArea, const int& destinationSIndex);
	TerrainMapSector* GetClosestSector(TerrainMapImmobileType* sourceIT, const int& destinationSIndex);
	TerrainMapAreaSector* GetAlternativeSector(TerrainMapArea* sourceArea, const int& sourceSIndex, TerrainMapMobileType* destinationMT);
	TerrainMapSector* GetAlternativeSector(TerrainMapArea* destinationArea, const int& sourceSIndex, TerrainMapImmobileType* destinationIT); // can return 0
	int GetSectorIndex(const float3& position); // use IsSectorValid() to insure the index is valid
	bool IsSectorValid(const int& sIndex);

	std::list<TerrainMapMobileType> mobileType;             // Used for mobile units, not all movedatas are used
	std::map<int, TerrainMapMobileType*> udMobileType;      // key = ud->id, Used to find a TerrainMapMobileType for a unit
	std::list<TerrainMapImmobileType> immobileType;         // Used for immobile units
	std::map<int, TerrainMapImmobileType*> udImmobileType;  // key = ud->id, Used to find a TerrainMapImmobileType for a unit
	std::vector<TerrainMapAreaSector> sectorAirType;        // used for flying units, GetSectorIndex gives an index
	std::vector<TerrainMapSector> sector;  // global sector data, GetSectorIndex gives an index
	TerrainMapImmobileType* landSectorType;   // 0 to the sky
	TerrainMapImmobileType* waterSectorType;  // minElevation to 0

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
