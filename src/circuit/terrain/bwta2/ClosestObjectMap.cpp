#include "ClosestObjectMap.h"
#include "Utils.h"
#include "BWTA_Result.h"

namespace BWTA
{
	template<typename T>
	inline void addToExplore(const int& x, const int& y, 
		const objectDistance_t<T>& current, std::queue<objectDistance_t<T>>& toExplore, const bool& jumpIncreaseCost)
	{
		// if change from walkable to not walkable, increase distance cost
		if (jumpIncreaseCost && MapData::walkability[current.x][current.y] && !MapData::walkability[x][y]) {
			toExplore.emplace(x, y, current.objectRef, current.distance + 100000);
		} else {
			toExplore.emplace(x, y, current.objectRef, current.distance);
		}
	}

	template<typename T>
	void computeClosestObjectMap(std::queue<objectDistance_t<T>> seedPositions, RectangleArray<T>& closestObjectMap, bool jumpIncreaseCost = false) {
		RectangleArray<double> minDistanceMap(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		minDistanceMap.setTo(-1);
		std::queue<objectDistance_t<T>> postionsToExplore(seedPositions);
		while (!postionsToExplore.empty()) {
			// pop first element
			objectDistance_t<T> check = postionsToExplore.front();
			postionsToExplore.pop();

			if (minDistanceMap[check.x][check.y] == -1 || minDistanceMap[check.x][check.y] > check.distance) {
				closestObjectMap[check.x][check.y] = check.objectRef;
				minDistanceMap[check.x][check.y] = check.distance;
				// look if the 8-connectivity are valid positions
				check.distance += 8; // straight move cost
				int left = check.x - 1;
				if (left >= 0) addToExplore(left, check.y, check, postionsToExplore, jumpIncreaseCost);
				int right = check.x + 1;
				if (right < MapData::mapWidthWalkRes) addToExplore(right, check.y, check, postionsToExplore, jumpIncreaseCost);
				int up = check.y - 1;
				if (up >= 0) addToExplore(check.x, up, check, postionsToExplore, jumpIncreaseCost);
				int down = check.y + 1;
				if (down < MapData::mapHeightWalkRes) addToExplore(check.x, down, check, postionsToExplore, jumpIncreaseCost);
				check.distance += 3; // increment on diagonal move cost
				if (left >= 0 && up >= 0) addToExplore(left, up, check, postionsToExplore, jumpIncreaseCost);
				if (left >= 0 && down < MapData::mapHeightWalkRes) addToExplore(left, down, check, postionsToExplore, jumpIncreaseCost);
				if (right < MapData::mapWidthWalkRes && up >= 0) addToExplore(right, up, check, postionsToExplore, jumpIncreaseCost);
				if (right < MapData::mapWidthWalkRes && down < MapData::mapHeightWalkRes) {
					addToExplore(right, down, check, postionsToExplore, jumpIncreaseCost);
				}
			}
		}
	}

	void computeAllClosestObjectMaps()
	{
		Timer timer;
		timer.start();

		// compute closest unwalkable polygon map
		// ===========================================================================
		BWTA_Result::closestObstacleLabelMap.resize(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		std::queue<labelDistance_t> seedLabels;
		for (const auto& pol : BWTA_Result::unwalkablePolygons) {
			int labelId = BWTA_Result::obstacleLabelMap[pol->front().x][pol->front().y];
			for (const auto& pos : *pol) seedLabels.emplace(pos.x, pos.y, labelId);
		}
		computeClosestObjectMap(seedLabels, BWTA_Result::closestObstacleLabelMap);
//		BWTA_Result::closestObstacleLabelMap.saveToFile(std::string(BWTA_PATH)+"closestObstacleMap.txt");
		
		LOG(" - Closest UnwalkablePolygonMap computed in " << timer.stopAndGetTime() << " seconds");
		timer.start();
		
		// compute closest BaseLocation map
		// ===========================================================================

		std::queue<baseDistance_t> seedPositions;
		for (const auto& baseLocation : BWTA_Result::baselocations) {
			seedPositions.emplace(baseLocation->getTilePosition().x * 4 + 8,
				baseLocation->getTilePosition().y * 4 + 6, baseLocation);
		}
		computeClosestObjectMap(seedPositions, BWTA_Result::getBaseLocationW, true);
		walkResMapToTileResMap(BWTA_Result::getBaseLocationW, BWTA_Result::getBaseLocation);

		LOG(" - Closest BaseLocation Map computed in " << timer.stopAndGetTime() << " seconds");
		timer.start();

		// compute closest Chokepoint map
		// ===========================================================================

		std::queue<chokeDistance_t> seedPositions2;
		for (const auto& chokepoint : BWTA_Result::chokepoints) {
			seedPositions2.emplace(chokepoint->getCenter().x / 8, chokepoint->getCenter().y / 8, chokepoint);
		}
		computeClosestObjectMap(seedPositions2, BWTA_Result::getChokepointW, true);
		walkResMapToTileResMap(BWTA_Result::getChokepointW, BWTA_Result::getChokepoint);

		LOG(" - Closest Chokepoint Map computed in " << timer.stopAndGetTime() << " seconds");
	}
}