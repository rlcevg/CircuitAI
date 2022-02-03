#include "MapData.h"

namespace BWTA
{
	namespace MapData
	{
		RectangleArray<bool> walkability;
		RectangleArray<bool> rawWalkability;
		RectangleArray<bool> lowResWalkability;
		RectangleArray<bool> buildability;
		RectangleArray<int> distanceTransform;
		BWAPI::TilePosition::list startLocations;
		std::string hash;
		std::string mapFileName;

		uint16_t mapWidthPixelRes;
		uint16_t mapWidthWalkRes;
		uint16_t mapWidthTileRes;
		uint16_t mapHeightPixelRes;
		uint16_t mapHeightWalkRes;
		uint16_t mapHeightTileRes;

		uint16_t maxDistanceTransform;
		// data for HPA*
		ChokepointGraph chokeNodes;
		
		// offline map data
		RectangleArray<bool> isWalkable;
		TileID   *TileArray = nullptr;
		TileType *TileSet   = nullptr;
		MiniTileMaps_type *MiniTileFlags = nullptr;
		std::vector<UnitTypePosition> staticNeutralBuildings;
//		std::vector<UnitTypeWalkPosition> resourcesWalkPositions;

		std::vector<resource_t> resources;
	}
}