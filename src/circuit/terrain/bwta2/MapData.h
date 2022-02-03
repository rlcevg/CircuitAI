#pragma once

#include <BWTA.h>
#include "TileType.h"

using TileID = uint16_t;

namespace BWTA
{
	typedef std::list<Chokepoint*> ChokePath;
	typedef std::set< std::pair<Chokepoint*, int> > ChokeCost;
	typedef std::map<Chokepoint*, ChokeCost> ChokepointGraph;

	typedef std::pair<BWAPI::UnitType, BWAPI::Position> UnitTypePosition;
//	typedef std::pair<BWAPI::UnitType, BWAPI::WalkPosition> UnitTypeWalkPosition;
//	typedef std::pair<BWAPI::UnitType, BWAPI::TilePosition> UnitTypeTilePosition;

	struct resource_t {
		BWAPI::UnitType type;
		BWAPI::TilePosition pos;
		unsigned int amount;
		bool isBlocking;

		resource_t(BWAPI::UnitType t, BWAPI::TilePosition p, unsigned int a): type(t), pos(p), amount(a), isBlocking(false) {};
	};

	namespace MapData
	{
		extern RectangleArray<bool> walkability;
		extern RectangleArray<bool> rawWalkability;
		extern RectangleArray<bool> lowResWalkability;
		extern RectangleArray<bool> buildability;
		extern RectangleArray<int> distanceTransform;
		extern BWAPI::TilePosition::list startLocations;
		extern std::string hash;
		extern std::string mapFileName;

		extern uint16_t mapWidthPixelRes;
		extern uint16_t mapWidthWalkRes;
		extern uint16_t mapWidthTileRes;
		extern uint16_t mapHeightPixelRes;
		extern uint16_t mapHeightWalkRes;
		extern uint16_t mapHeightTileRes;

		extern uint16_t maxDistanceTransform;
		// data for HPA*
		extern ChokepointGraph chokeNodes;
		
		// offline map data
		extern TileID   *TileArray;
		extern TileType *TileSet;
		/** Direct mapping of mini tile flags array */
		struct MiniTileMaps_type {
			struct MiniTileFlagArray {
				uint16_t miniTile[16];
			};
			MiniTileFlagArray tile[0x10000];
		};
		extern MiniTileMaps_type *MiniTileFlags;
		extern std::vector<UnitTypePosition> staticNeutralBuildings;
//		extern std::vector<UnitTypeWalkPosition> resourcesWalkPositions;

		extern std::vector<resource_t> resources;
	}
}


