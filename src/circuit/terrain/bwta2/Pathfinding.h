#pragma once

#include "Heap.h"
#include "MapData.h"

namespace BWTA
{
	double AstarSearchDistance(BWAPI::TilePosition start, BWAPI::TilePosition end);
	std::pair<BWAPI::TilePosition, double> AstarSearchDistance(BWAPI::TilePosition start, std::set<BWAPI::TilePosition>& end);
	std::map<BWAPI::TilePosition, double> AstarSearchDistanceAll(BWAPI::TilePosition start, std::set<BWAPI::TilePosition>& end);
	std::vector<BWAPI::TilePosition> AstarSearchPath(BWAPI::TilePosition start, BWAPI::TilePosition end);
	std::vector<BWAPI::TilePosition> AstarSearchPath(BWAPI::TilePosition start, std::set<BWAPI::TilePosition> end);
}