#include "BWTA_Result.h"

namespace BWTA
{
	namespace BWTA_Result
	{
		std::vector<Region*> regions;
		std::set<Chokepoint*> chokepoints;
		std::set<BaseLocation*> baselocations;
		std::set<BaseLocation*> startlocations;
		std::vector<Polygon*> unwalkablePolygons;

		RectangleArray<Region*> getRegion;
		RectangleArray<Polygon*> getUnwalkablePolygon;
		RectangleArray<Chokepoint*> getChokepointW;
		RectangleArray<Chokepoint*> getChokepoint;
		RectangleArray<BaseLocation*> getBaseLocationW;
		RectangleArray<BaseLocation*> getBaseLocation;

		RectangleArray<int> obstacleLabelMap;
		RectangleArray<int> closestObstacleLabelMap;
		RectangleArray<int> regionLabelMap;	// stores the region ID in walk resolution
	};
}