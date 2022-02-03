#pragma once

#include <BWTA.h>

namespace BWTA
{
	namespace BWTA_Result
	{
		// List of objects extracted
		// TODO instead of set use vector
		extern std::vector<Region*> regions;
		extern std::set<Chokepoint*> chokepoints;
		extern std::set<BaseLocation*> baselocations;
		extern std::set<BaseLocation*> startlocations;
		extern std::vector<Polygon*> unwalkablePolygons;

		// Distance Map to closest elements (by defaults in Tile resolution, W = Walk resolution)
		extern RectangleArray<Region*> getRegion; // TODO remove, use regionLabelMap instead
		extern RectangleArray<Polygon*> getUnwalkablePolygon;
		extern RectangleArray<Chokepoint*> getChokepointW;
		extern RectangleArray<Chokepoint*> getChokepoint;
		extern RectangleArray<BaseLocation*> getBaseLocationW;
		extern RectangleArray<BaseLocation*> getBaseLocation;

		// TODO save this data
		extern RectangleArray<int> obstacleLabelMap;
		extern RectangleArray<int> closestObstacleLabelMap;
		extern RectangleArray<int> regionLabelMap;
		// TODO add closestRegionLabelMap
	};
}