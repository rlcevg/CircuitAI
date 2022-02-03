#include <BWTA.h>
#include "BWTA_Result.h"
#include "Heap.h"
#include "MapData.h"
#include "Pathfinding.h"
#include "stdafx.h"

namespace BWTA
{
	void cleanMemory()
	{
		// clear everything
		for (auto r : BWTA_Result::regions) delete r;
		BWTA_Result::regions.clear();
		for (auto c : BWTA_Result::chokepoints) delete c;
		BWTA_Result::chokepoints.clear();
		for (auto p : BWTA_Result::unwalkablePolygons) delete p;
		BWTA_Result::unwalkablePolygons.clear();
		for (auto b : BWTA_Result::baselocations) delete b;
		BWTA_Result::baselocations.clear();
		BWTA_Result::startlocations.clear();
	}

	const std::vector<Region*>& getRegions()			{ return BWTA_Result::regions; }
	const std::set<Chokepoint*>& getChokepoints()		{ return BWTA_Result::chokepoints; }
	const std::set<BaseLocation*>& getBaseLocations()	{ return BWTA_Result::baselocations; }
	const std::set<BaseLocation*>& getStartLocations()	{ return BWTA_Result::startlocations; }
	const std::vector<Polygon*>& getUnwalkablePolygons()	{ return BWTA_Result::unwalkablePolygons; }

	BWAPI::Position getNearestUnwalkablePosition(BWAPI::Position position)
	{
		Polygon* p = BWTA::getNearestUnwalkablePolygon(position.x / 32, position.y / 32);
		BWAPI::Position nearest = BWAPI::Positions::None;
		if (p == NULL) {
			//use an edge of the map if we don't find a polygon
			nearest = BWAPI::Position(0, position.y);
		} else {
			nearest = p->getNearestPoint(position);
		}
		if (position.x < position.getDistance(nearest))
			nearest = BWAPI::Position(0, position.y);
		if (position.y < position.getDistance(nearest))
			nearest = BWAPI::Position(position.x, 0);
		if (MapData::mapWidthPixelRes - position.x < position.getDistance(nearest))
			nearest = BWAPI::Position(MapData::mapWidthPixelRes, position.y);
		if (MapData::mapHeightPixelRes - position.y < position.getDistance(nearest))
			nearest = BWAPI::Position(position.x, MapData::mapHeightPixelRes);
		return nearest;
	}

	BaseLocation* getStartLocation(BWAPI::Player player)
	{
		if (player == nullptr) return nullptr;
		return getNearestBaseLocation(player->getStartLocation());
	}

	Region* getRegion(int x, int y) { return getRegion(BWAPI::TilePosition(x, y)); }
	Region* getRegion(BWAPI::Position pos) { return getRegion(BWAPI::WalkPosition(pos)); }
	Region* getRegion(BWAPI::TilePosition tilePos) { return getRegion(BWAPI::WalkPosition(tilePos)); }
	Region* getRegion(BWAPI::WalkPosition walkPos)
	{
		if (walkPos.x<0 || walkPos.y<0 || 
			walkPos.x>=BWTA_Result::regionLabelMap.getWidth() || walkPos.y>=BWTA_Result::regionLabelMap.getHeight()) {
				LOG("WARNING getRegion called with wrong WalkPosition " << walkPos);
				return nullptr;
		}
		int regionLabel = BWTA_Result::regionLabelMap[walkPos.x][walkPos.y];
		for (const auto& r : BWTA_Result::regions) {
			if (r->getLabel() == regionLabel) { // TODO I need a vector to map labelId to Region*
				return r;
			}
		}

		// TODO if 0 return closest region??
		return nullptr;
	}

	Chokepoint* getNearestChokepoint(int x, int y)
	{
		return BWTA_Result::getChokepoint.getItemSafe(x, y);
	}
	Chokepoint* getNearestChokepoint(BWAPI::TilePosition position)
	{
		return BWTA_Result::getChokepoint.getItemSafe(position.x, position.y);
	}
	Chokepoint* getNearestChokepoint(BWAPI::Position position)
	{
		return BWTA_Result::getChokepointW.getItemSafe(position.x / 8, position.y / 8);
	}
	BaseLocation* getNearestBaseLocation(int x, int y)
	{
		return BWTA_Result::getBaseLocation.getItemSafe(x, y);
	}
	BaseLocation* getNearestBaseLocation(BWAPI::TilePosition tileposition)
	{
		return BWTA_Result::getBaseLocation.getItemSafe(tileposition.x, tileposition.y);
	}
	BaseLocation* getNearestBaseLocation(BWAPI::Position position)
	{
		return BWTA_Result::getBaseLocationW.getItemSafe(position.x / 8, position.y / 8);
	}
	Polygon* getNearestUnwalkablePolygon(int x, int y)
	{
		return BWTA_Result::getUnwalkablePolygon.getItemSafe(x, y);
	}
	Polygon* getNearestUnwalkablePolygon(BWAPI::TilePosition tileposition)
	{
		return BWTA_Result::getUnwalkablePolygon.getItemSafe(tileposition.x, tileposition.y);
	}

	bool isConnected(int x1, int y1, int x2, int y2)
	{
		return isConnected(BWAPI::TilePosition(x1, y1), BWAPI::TilePosition(x2, y2));
	}
	bool isConnected(BWAPI::TilePosition a, BWAPI::TilePosition b)
	{
		Region* r1 = getRegion(a);
		Region* r2 = getRegion(b);
		if (r1 == nullptr || r2 == nullptr) return false;
		return r1->isReachable(r2);
	}
	std::pair<BWAPI::TilePosition, double> getNearestTilePosition(BWAPI::TilePosition start, const std::set<BWAPI::TilePosition>& targets)
	{
		std::set<BWAPI::TilePosition> valid_targets;
		for (std::set<BWAPI::TilePosition>::const_iterator i = targets.begin(); i != targets.end(); i++)
		{
			if (isConnected(start, *i))
				valid_targets.insert(*i);
		}
		if (valid_targets.empty())
			return std::make_pair(BWAPI::TilePositions::None, -1);
		return AstarSearchDistance(start, valid_targets);
	}
	double getGroundDistance(BWAPI::TilePosition start, BWAPI::TilePosition end)
	{
		if (!isConnected(start, end)) return -1;
		return AstarSearchDistance(start, end);
	}
	std::map<BWAPI::TilePosition, double> getGroundDistances(BWAPI::TilePosition start, const std::set<BWAPI::TilePosition>& targets)
	{
		std::map<BWAPI::TilePosition, double> answer;
		std::set<BWAPI::TilePosition> valid_targets;
		for (std::set<BWAPI::TilePosition>::const_iterator i = targets.begin(); i != targets.end(); i++)
		{
			if (isConnected(start, *i)) valid_targets.insert(*i);
			else answer[*i] = -1;
		}
		if (valid_targets.empty()) return answer;
		return AstarSearchDistanceAll(start, valid_targets);
	}
	void getGroundDistanceMap(BWAPI::TilePosition start, RectangleArray<double>& distanceMap)
	{
		distanceMap.resize(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		Heap< BWAPI::TilePosition, int > heap(true);
		for (unsigned int x = 0; x < distanceMap.getWidth(); x++) {
			for (unsigned int y = 0; y < distanceMap.getHeight(); y++) {
				distanceMap[x][y] = -1;
			}
		}
		heap.push(std::make_pair(start, 0));
		int sx = (int)start.x;
		int sy = (int)start.y;
		distanceMap[sx][sy] = 0;
		while (!heap.empty()) {
			BWAPI::TilePosition pos = heap.top().first;
			int distance = heap.top().second;
			heap.pop();
			int x = (int)pos.x;
			int y = (int)pos.y;
			int min_x = std::max(x - 1, 0);
			int max_x = std::min(x + 1, (int)distanceMap.getWidth() - 1);
			int min_y = std::max(y - 1, 0);
			int max_y = std::min(y + 1, (int)distanceMap.getHeight() - 1);
			for (int ix = min_x; ix <= max_x; ix++) {
				for (int iy = min_y; iy <= max_y; iy++) {
					int f = std::abs(ix - x) * 32 + std::abs(iy - y) * 32;
					if (f > 32) { f = 45; }
					int v = distance + f;
					if (distanceMap[ix][iy] > v) {
						heap.push(std::make_pair(BWAPI::TilePosition(x, y), v));
						distanceMap[ix][iy] = v;
					} else {
						if (distanceMap[ix][iy] == -1 && MapData::lowResWalkability[ix][iy] == true) {
							distanceMap[ix][iy] = v;
							heap.push(std::make_pair(BWAPI::TilePosition(ix, iy), v));
						}
					}
				}
			}
		}
	}
	std::vector<BWAPI::TilePosition> getShortestPath(BWAPI::TilePosition start, BWAPI::TilePosition end)
	{
		std::vector<BWAPI::TilePosition> path;
		if (!isConnected(start, end)) return path;
		return AstarSearchPath(start, end);
	}
	std::vector<BWAPI::TilePosition> getShortestPath(BWAPI::TilePosition start, const std::set<BWAPI::TilePosition>& targets)
	{
		std::vector<BWAPI::TilePosition> path;
		std::set<BWAPI::TilePosition> valid_targets;
		for (std::set<BWAPI::TilePosition>::const_iterator i = targets.begin(); i != targets.end(); i++)
		{
			if (isConnected(start, *i))
				valid_targets.insert(*i);
		}
		if (valid_targets.empty())
			return path;
		return AstarSearchPath(start, valid_targets);
	}

	int getMaxDistanceTransform()
	{
		return MapData::maxDistanceTransform;
	}

	RectangleArray<int>* getDistanceTransformMap()
	{
		return &MapData::distanceTransform;
	}
}