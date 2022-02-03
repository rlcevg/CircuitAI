#include "BaseLocationGenerator.h"

#include "BaseLocationImpl.h"
#include "RegionImpl.h"
#include "Heap.h"

namespace BWTA
{
	const int MIN_CLUSTER_DIST = 8; // minerals less than this distance will be grouped into the same cluster
	const size_t MIN_RESOURCES = 3; // a cluster with less than this will be discarded
	const int MAX_INFLUENCE_DISTANCE_RADIUS = 10; // max radius distance from a resource to place a base

	// TODO review this method, probably we can optimize it
	void calculate_walk_distances_area(const BWAPI::Position& start, int width, int height, int max_distance, 
		RectangleArray<int>& distance_map)
	{
		distance_map.setTo(-1);

		Heap<BWAPI::Position, int> heap(true);
		const int maxX = start.x + width;
		const int maxY = start.y + height;
		for (int x = start.x; x < maxX; ++x) {
			for (int y = start.y; y < maxY; ++y) {
				heap.push(std::make_pair(BWAPI::Position(x, y), 0));
				distance_map[x][y] = 0;
			}
		}

		while (!heap.empty()) {
			BWAPI::Position pos = heap.top().first;
			int distance = heap.top().second;
			heap.pop();
			if (distance > max_distance && max_distance > 0) break;
			int min_x = std::max(pos.x - 1, 0);
			int max_x = std::min(pos.x + 1, MapData::mapWidthWalkRes - 1);
			int min_y = std::max(pos.y - 1, 0);
			int max_y = std::min(pos.y + 1, MapData::mapHeightWalkRes - 1);
			for (int ix = min_x; ix <= max_x; ++ix) {
				for (int iy = min_y; iy <= max_y; ++iy) {
					int f = std::abs(ix - pos.x) * 10 + std::abs(iy - pos.y) * 10;
					if (f > 10) { f = 14; }
					int v = distance + f;
					if (distance_map[ix][iy] > v) {
						heap.push(std::make_pair(pos, v));
						distance_map[ix][iy] = v;
					} else if (distance_map[ix][iy] == -1 && MapData::rawWalkability[ix][iy] == true) {
						heap.push(std::make_pair(BWAPI::Position(ix, iy), v));
						distance_map[ix][iy] = v;
					}
				}
			}
		}
	}

	std::vector<int> findNeighbors(const std::vector<resource_t>& resources, const resource_t& resource)
	{
		std::vector<int> retIndexes;
		for (size_t i = 0; i < resources.size(); ++i) {
			int dist = resources[i].pos.getApproxDistance(resource.pos);
			if (dist <= MIN_CLUSTER_DIST) retIndexes.emplace_back(i);
		}
		return retIndexes;
	}

	BWAPI::TilePosition getBestTile(const RectangleArray<int>& tileScores, int minX, int maxX, int minY, int maxY) 
	{
		BWAPI::TilePosition bestTile = BWAPI::TilePositions::None;
		int maxScore = 0;
		for (int x = minX; x < maxX; ++x)
		for (int y = minY; y < maxY; ++y) {
			if (tileScores[x][y] > maxScore) {
				maxScore = tileScores[x][y];
				bestTile = BWAPI::TilePosition(x, y);
			}
		}
		return bestTile;
	}

	void detectBaseLocations(std::set<BaseLocation*>& baseLocations)
	{
		Timer timer;
		timer.start();

		// 1) cluster resources using DBSCAN algorithm
		// ===========================================================================

		std::vector<std::vector<resource_t>> clusters;
		std::vector<bool> clustered(MapData::resources.size());
		std::vector<bool> visited(MapData::resources.size());
		int clusterID = -1;

		// for each unvisited resource
		for (size_t i = 0; i < MapData::resources.size(); ++i) {
			if (!visited[i]) {
				visited[i] = true;
				std::vector<int> neighbors = findNeighbors(MapData::resources, MapData::resources[i]);
				if (neighbors.size() >= MIN_RESOURCES) {
					// add resource to a new cluster
					clusters.emplace_back(std::vector<resource_t> {MapData::resources[i]});
					++clusterID;
					clustered[i] = true;

					for (size_t j = 0; j < neighbors.size(); ++j) {
						size_t neighborID = neighbors[j];
						if (!visited[neighborID]) {
							visited[neighborID] = true;
							std::vector<int> neighbors2 = findNeighbors(MapData::resources, MapData::resources[neighborID]);
							if (neighbors2.size() >= MIN_RESOURCES) {
								neighbors.insert(neighbors.end(), neighbors2.begin(), neighbors2.end());
							} else {
//								LOG("Resource " << neighborID << ":" << MapData::resources[neighborID].type << 
//									" only has " << neighbors2.size() << " neighbors");
							}
						}
						// if neighbor is not yet a member of any cluster
						if (!clustered[neighborID]) { 
							// add neighbor to current cluster
							clusters[clusterID].emplace_back(MapData::resources[neighborID]);
							clustered[neighborID] = true;
						}
					}
				} else {
//					LOG("Resource " << i << ":" << MapData::resources[i].type << 
//						" only has " << neighbors.size() << " neighbors");
				}
			}
		}
//		for (const auto& c : clusters) LOG("  - Cluster size: " << c.size());
		LOG(" - Found " << clusters.size() << " resource clusters in " << timer.stopAndGetTime() << " seconds");
		timer.start();

		// 2) compute a buildable map where a resource depot can be build (4x3 tiles)
		// ===========================================================================

		RectangleArray<bool> baseBuildMap(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		// baseBuildMap[x][y] is true if build_map[ix][iy] is true for all x<=ix<x+4 and all y<=yi<+3
		// 4 and 3 are the tile width and height of a command center/nexus/hatchery
		for (unsigned int x = 0; x < MapData::buildability.getWidth(); ++x) {
			for (unsigned int y = 0; y < MapData::buildability.getHeight(); ++y) {
				// If this tile is too close to the bottom or right of the map, set it to false
				if (x + 4 > MapData::buildability.getWidth() || y + 3 > MapData::buildability.getHeight()) {
					baseBuildMap[x][y] = false;
					continue;
				}

				baseBuildMap[x][y] = true; // by default is buildable

				unsigned int maxX = std::min(x + 4, MapData::buildability.getWidth());
				unsigned int maxY = std::min(y + 3, MapData::buildability.getHeight());
				for (unsigned int ix = x; ix < maxX; ++ix) {
					for (unsigned int iy = y; iy < maxY; ++iy) {
						// if we found one non buildable tile, all the area (4x3) is  not buildable
						if (!MapData::buildability[ix][iy]) {
							baseBuildMap[x][y] = false;
							break;
						}
					}
				}

				
			}
		}
		// Set build tiles too close to resources in any cluster to false in baseBuildMap
		for (auto& cluster : clusters) {
			for (auto& resource : cluster) {
				if (resource.amount > 200) {
					int x1 = resource.pos.x - 6; // base (4-1) + starcraft rule (3)
					int y1 = resource.pos.y - 5; // base (3-1) + starcraft rule (3)
					int x2 = resource.pos.x + resource.type.tileWidth() + 2;
					int y2 = resource.pos.y + resource.type.tileHeight() + 2;
					baseBuildMap.setRectangleTo(x1, y1, x2, y2, false);
				} else {
					resource.isBlocking = true;
				}
			}
		}
		LOG(" - baseBuildMap computed in " << timer.stopAndGetTime() << " seconds");
		timer.start();

		// 3) with the clusters and baseBuildMap, we will try to find a base location for each cluster
		// ===========================================================================

		RectangleArray<int> tileScores(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		tileScores.setTo(0);
		const int maxWidth = static_cast<int>(baseBuildMap.getWidth()) - 1;
		const int maxHeight = static_cast<int>(baseBuildMap.getHeight()) - 1;
		const int maxSquareDistance = MAX_INFLUENCE_DISTANCE_RADIUS*MAX_INFLUENCE_DISTANCE_RADIUS*2;

		// copy of start locations
		std::deque<BWAPI::TilePosition> startLocations = MapData::startLocations;

		for (const auto& cluster : clusters) {
			// if all resources are empty, they are blocking a path
			bool allEmpty = true;
			for (const auto& r : cluster) {
				if (r.amount > 200) {
					allEmpty = false;
					break;
				}
			}
			if (allEmpty) {
				// TODO update blocking path
				LOG(" - [WARNING] Skipped 'empty' cluster, probably mineral blocking path");
				continue;
			}
			int clusterMaxX = 0;
			int clusterMinX = maxWidth;
			int clusterMaxY = 0;
			int clusterMinY = maxHeight;
			// get the region label from the first resource
			BWAPI::WalkPosition clusterWalkPos(cluster.front().pos);
			// hotfix to avoid top row (since it's always marked as unwalkable) TODO check why
			int tmpY = std::max (1,clusterWalkPos.y);
			int clusterRegionLabel = BWTA_Result::regionLabelMap[clusterWalkPos.x][tmpY];
//			LOG("Cluster RegionLabelID: " << clusterRegionLabel << " at Tile:" << cluster.front().pos << " walk:" << clusterWalkPos);
			if (clusterRegionLabel == 0) {
				LOG(" - [ERROR] Cluster in an unwalkable region");
				continue;
			}

			for (const auto& resource : cluster) {
//				LOG(" - " << resource.type << " at " << resource.pos << " amount resources: " << resource.amount);
				if (resource.amount <= 200) continue;
				// TODO add methods to update bounding boxes
				// bounding box of current resource influence
				BWAPI::TilePosition topLeft(resource.pos);
				BWAPI::TilePosition bottomRight(resource.pos.x + resource.type.tileWidth()-1, resource.pos.y + resource.type.tileHeight()-1);

				// bounding box of current resource influence
				int minX = std::max(topLeft.x	  - MAX_INFLUENCE_DISTANCE_RADIUS, 0);
				int maxX = std::min(bottomRight.x + MAX_INFLUENCE_DISTANCE_RADIUS, maxWidth);
				int minY = std::max(topLeft.y	  - MAX_INFLUENCE_DISTANCE_RADIUS, 0);
				int maxY = std::min(bottomRight.y + MAX_INFLUENCE_DISTANCE_RADIUS, maxHeight);

//				tileScores.setRectangleTo(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y, -1);
//				if (resource.type != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;

				// a geyser has more "weight" than minerals
				int scoreScale = resource.type == BWAPI::UnitTypes::Resource_Vespene_Geyser? 3 : 1;

				// updating score inside the bounding box
				for (int x = minX; x < maxX; ++x) {
					for (int y = minY; y < maxY; ++y) {
						if (baseBuildMap[x][y] && clusterRegionLabel == BWTA_Result::regionLabelMap[x*4][y*4]) {
//						if (clusterRegionLabel == BWTA_Result::regionLabelMap[x*4][y*4]) {
							int dx = 0;
							// check if x is outside resource bounding box
							if (x+3 < topLeft.x) dx = topLeft.x - (x+3);
							else if (x > bottomRight.x) dx = x - bottomRight.x;
							int dy = 0;
							// check if y is outside resource boudning box
							if (y+2 < topLeft.y) dy = topLeft.y - (y+2);
							else if (y > bottomRight.y) dy = y - bottomRight.y;
							// since we only need to compare distances the squareDistance is enough
							int squareDistance = dx*dx + dy*dy; 
							tileScores[x][y] += (maxSquareDistance - squareDistance) * scoreScale;
						}
					}
				}
				
				// update the bounding box of the cluster
				clusterMaxX = std::max(maxX, clusterMaxX);
				clusterMinX = std::min(minX, clusterMinX);
				clusterMaxY = std::max(maxY, clusterMaxY);
				clusterMinY = std::min(minY, clusterMinY);
			}

			BWAPI::TilePosition bestTile = getBestTile(tileScores, clusterMinX, clusterMaxX, clusterMinY, clusterMaxY);

			if (bestTile != BWAPI::TilePositions::None) {
				// if there is already a baseLoctaion too close, merge them
				bool baseLocationMerged = false;
				for (auto& b : baseLocations) {
					if (b->getTilePosition().getApproxDistance(bestTile) < 5) {
						BaseLocationImpl* bi = static_cast<BaseLocationImpl*>(b);
						int minX = std::min(bestTile.x, b->getTilePosition().x);
						int maxX = std::max(bestTile.x, b->getTilePosition().x);
						int minY = std::min(bestTile.y, b->getTilePosition().y);
						int maxY = std::max(bestTile.y, b->getTilePosition().y);
						bi->setTile(getBestTile(tileScores, minX-5, maxX+5, minY-5, maxY+5));
						bi->resources.insert(bi->resources.end(), cluster.begin(), cluster.end());
						baseLocationMerged = true;
//						LOG(" - Two baseLocation merged");
						break;
					}
				}
				if (!baseLocationMerged) {
					BaseLocationImpl* b = new BaseLocationImpl(bestTile, cluster);
					baseLocations.insert(b);
					// check if it is a start location
					for (auto it = startLocations.begin(); it != startLocations.end(); ++it) {
						int distance = it->getApproxDistance(bestTile);
						if (distance < MAX_INFLUENCE_DISTANCE_RADIUS) {
							b->_isStartLocation = true;
							BWTA_Result::startlocations.insert(b);
							startLocations.erase(it);
							break;
						}
					}
				}
			} else {
				LOG(" - [ERROR] No BaseLocation found for a cluster");
			}
		}
//		BWTA_Result::regionLabelMap.saveToFile(std::string(BWTA_PATH)+"regionLabelMap.txt", ',');
//		tileScores.saveToFile(std::string(BWTA_PATH)+"tileScores.txt", ',');
		if (!startLocations.empty()) LOG(" - [ERROR] " << startLocations.size() << " start locations not found.");


		LOG(" - Best baseLocations computed in " << timer.stopAndGetTime() << " seconds");
	}



	//attach resource pointers to base locations based on proximity (walk distance)
	void attachResourcePointersToBaseLocations(std::set<BWTA::BaseLocation*>& baseLocations)
	{
		RectangleArray<int> distanceMap(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		for (auto& b : baseLocations) {
			BWAPI::Position p(b->getTilePosition().x * 4, b->getTilePosition().y * 4);
			calculate_walk_distances_area(p, 16, 12, 10 * 4 * 10, distanceMap);
			BWTA::BaseLocationImpl* ii = static_cast<BWTA::BaseLocationImpl*>(b);
			
			for (auto geyser : BWAPI::Broodwar->getStaticGeysers()) {
				int x = geyser->getInitialTilePosition().x * 4 + 8;
				int y = geyser->getInitialTilePosition().y * 4 + 4;
				if (distanceMap[x][y] >= 0 && distanceMap[x][y] <= 4 * 10 * 10) {
					ii->geysers.insert(geyser);
				}
			}
			
			for (auto mineral : BWAPI::Broodwar->getStaticMinerals()) {
				int x = mineral->getInitialTilePosition().x * 4 + 4;
				int y = mineral->getInitialTilePosition().y * 4 + 2;
				if (distanceMap[x][y] >= 0 && distanceMap[x][y] <= 4 * 10 * 10) {
					ii->staticMinerals.insert(mineral);
				}
			}
		}
	}

	void calculateBaseLocationProperties()
	{
		RectangleArray<double> distanceMap; 
		for (auto& base : BWTA_Result::baselocations) {
			BaseLocationImpl* baseI = static_cast<BaseLocationImpl*>(base);

			// TODO this can be optimized only computing the distance between reachable base locations
			BWAPI::TilePosition baseTile = base->getTilePosition();
			getGroundDistanceMap(baseTile, distanceMap);
			// assume the base location is an island unless we can walk from this base location to another base location
			for (const auto& base2 : BWTA_Result::baselocations) {
				if (base == base2) {
					baseI->groundDistances[base2] = 0;
					baseI->airDistances[base2] = 0;
				} else {
					BWAPI::TilePosition base2Tile = base2->getTilePosition();
					if (baseI->_isIsland && isConnected(baseTile, base2Tile)) {
						baseI->_isIsland = false;
					}
					baseI->groundDistances[base2] = distanceMap[base2Tile.x][base2Tile.y];
					baseI->airDistances[base2] = baseTile.getDistance(base2Tile);
				}
			}

			// find what region this base location is in and tell that region about the base location
			BWAPI::WalkPosition baseWalkPos(base->getPosition());
			int baseRegionLabel = BWTA_Result::regionLabelMap[baseWalkPos.x][baseWalkPos.y];
			for (auto& r : BWTA_Result::regions) {
				if (r->getLabel() == baseRegionLabel) { // TODO I need a vector to map labelId to Region*
					baseI->region = r;
					static_cast<RegionImpl*>(r)->baseLocations.insert(base);
					break;
				}
			}

		}
	}
}