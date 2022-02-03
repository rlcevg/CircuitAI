#pragma once
#include <BWAPI.h>
#include <BWTA/Chokepoint.h>
#include <BWTA/Polygon.h>
#include <BWTA/Region.h>
#include <BWTA/BaseLocation.h>
#include <BWTA/RectangleArray.h>
namespace BWTA
{
  void analyze(); // will check if we can load cache file or we need to analyze the map
  void computeDistanceTransform();
  void balanceAnalysis();
  void cleanMemory();

  int getMaxDistanceTransform();
  RectangleArray<int>* getDistanceTransformMap();

  const std::vector<Region*>& getRegions();
  const std::set<Chokepoint*>& getChokepoints();
  const std::set<BaseLocation*>& getBaseLocations();
  const std::set<BaseLocation*>& getStartLocations();
  const std::vector<Polygon*>& getUnwalkablePolygons();

  BaseLocation* getStartLocation(BWAPI::Player player);

  Region* getRegion(int x, int y); // in TilePosition
  Region* getRegion(BWAPI::Position pos);
  Region* getRegion(BWAPI::WalkPosition walkPos);
  Region* getRegion(BWAPI::TilePosition tilePos);

  Chokepoint* getNearestChokepoint(int x, int y);
  Chokepoint* getNearestChokepoint(BWAPI::TilePosition tileposition);
  Chokepoint* getNearestChokepoint(BWAPI::Position position);

  BaseLocation* getNearestBaseLocation(int x, int y);
  BaseLocation* getNearestBaseLocation(BWAPI::TilePosition tileposition);
  BaseLocation* getNearestBaseLocation(BWAPI::Position position);

  Polygon* getNearestUnwalkablePolygon(int x, int y);
  Polygon* getNearestUnwalkablePolygon(BWAPI::TilePosition tileposition);
  BWAPI::Position getNearestUnwalkablePosition(BWAPI::Position position);

  bool isConnected(int x1, int y1, int x2, int y2); // in TilePosition
  bool isConnected(BWAPI::TilePosition a, BWAPI::TilePosition b);

  double getGroundDistance(BWAPI::TilePosition start, BWAPI::TilePosition end);
  std::pair<BWAPI::TilePosition, double> getNearestTilePosition(BWAPI::TilePosition start, const std::set<BWAPI::TilePosition>& targets);
  std::map<BWAPI::TilePosition, double> getGroundDistances(BWAPI::TilePosition start, const std::set<BWAPI::TilePosition>& targets);
  void getGroundDistanceMap(BWAPI::TilePosition start, RectangleArray<double>& distanceMap);
  std::vector<BWAPI::TilePosition> getShortestPath(BWAPI::TilePosition start, BWAPI::TilePosition end);
  std::vector<BWAPI::TilePosition> getShortestPath(BWAPI::TilePosition start, const std::set<BWAPI::TilePosition>& targets);

   // HPA* implementation
  void buildChokeNodes();
  std::list<Chokepoint*> getShortestPath2(BWAPI::TilePosition start, BWAPI::TilePosition target);
  int getGroundDistance2(BWAPI::TilePosition start, BWAPI::TilePosition end);

}