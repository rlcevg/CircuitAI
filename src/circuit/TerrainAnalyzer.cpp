/*
 * TerrainAnalyzer.cpp
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#include "TerrainAnalyzer.h"
#include "CircuitAI.h"
#include "MetalManager.h"
#include "utils.h"

#include "Map.h"
#include "UnitDef.h"

#include <algorithm>

namespace circuit {

using namespace springai;

CTerrainAnalyzer::CTerrainAnalyzer(CCircuitAI* circuit) :
		circuit(circuit)
{
	Map* map = circuit->GetMap();
	terrainWidth = map->GetWidth() * SQUARE_SIZE;
	terrainHeight = map->GetHeight() * SQUARE_SIZE;

//	// debug
//	if (circuit->GetSkirmishAIId() != 1) {
//		return;
//	}
//	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>([circuit]() {
//		// step 1: Create waypoints
//		Pathing* pathing = circuit->GetPathing();
//		Map* map = circuit->GetMap();
//		const CMetalManager::Metals& spots = circuit->GetMetalManager().GetSpots();
//		std::vector<AIFloat3> points;
//		for (auto& spot : spots) {
//			AIFloat3 start = spot.position;
//			for (auto& s : spots) {
//				if (spot.position == s.position) {
//					continue;
//				}
//				AIFloat3 end = s.position;
//				int pathId = pathing->InitPath(start, end, 4, .0f);
//				AIFloat3 lastPoint, point(start);
////				Drawer* drawer = map->GetDrawer();
//				int j = 0;
//				do {
//					lastPoint = point;
//					point = pathing->GetNextWaypoint(pathId);
//					if (point.x <= 0 || point.z <= 0) {
//						break;
//					}
////					drawer->AddLine(lastPoint, point);
//					if (j++ % 25 == 0) {
//						points.push_back(point);
//					}
//				} while ((lastPoint != point) && (point.x > 0 && point.z > 0));
////				delete drawer;
//				pathing->FreePath(pathId);
//			}
//		}
//
//		// step 2: Create path traversability map
//		// @see path traversability map rts/
//		int widthX = circuit->GetMap()->GetWidth();
//		int heightZ = circuit->GetMap()->GetHeight();
//		int widthSX = widthX / 2;
//		MoveData* moveDef = circuit->GetUnitDefByName("armcom1")->GetMoveData();
//		float maxSlope = moveDef->GetMaxSlope();
//		float depth = moveDef->GetDepth();
//		float slopeMod = moveDef->GetSlopeMod();
//		std::vector<float> heightMap = circuit->GetMap()->GetHeightMap();
//		std::vector<float> slopeMap = circuit->GetMap()->GetSlopeMap();
//
//		std::vector<float> traversMap(widthX * heightZ);
//
//		auto posSpeedMod = [](float maxSlope, float depth, float slopeMod, float depthMod, float height, float slope) {
//			float speedMod = 0.0f;
//
//			// slope too steep or square too deep?
//			if (slope > maxSlope)
//				return speedMod;
//			if (-height > depth)
//				return speedMod;
//
//			// slope-mod
//			speedMod = 1.0f / (1.0f + slope * slopeMod);
//			// FIXME: Read waterDamageCost from mapInfo
////			speedMod *= (height < 0.0f) ? waterDamageCost : 1.0f;
//			speedMod *= depthMod;
//
//			return speedMod;
//		};
//
//		for (int hz = 0; hz < heightZ; ++hz) {
//			for (int hx = 0; hx < widthX; ++hx) {
//				const int sx = hx / 2;  // hx >> 1;
//				const int sz = hz / 2;  // hz >> 1;
////				const bool losSqr = losHandler->InLos(sqx, sqy, gu->myAllyTeam);
//
//				float scale = 1.0f;
//
//				// TODO: First implement blocking map
////				if (los || losSqr) {
////					if (CMoveMath::IsBlocked(*md, sqx,     sqy    , NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
////					if (CMoveMath::IsBlocked(*md, sqx + 1, sqy    , NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
////					if (CMoveMath::IsBlocked(*md, sqx,     sqy + 1, NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
////					if (CMoveMath::IsBlocked(*md, sqx + 1, sqy + 1, NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
////				}
//
//				int idx = hz * widthX + hx;
//				float height = heightMap[idx];
//				float slope = slopeMap[sz * widthSX + sx];
//				float depthMod = moveDef->GetDepthMod(height);
//				traversMap[idx] = posSpeedMod(maxSlope, depth, slopeMod, depthMod, height, slope);
//				// FIXME: blocking map first
////				const SColor& smc = GetSpeedModColor(sm * scale);
//			}
//		}
//		delete moveDef;
//
//		// step 3: Filter key waypoints
//		printf("points size: %i\n", points.size());
//		auto iter = points.begin();
//		while (iter != points.end()) {
//			bool isKey = false;
//			if ((iter->z / SQUARE_SIZE - 10 >= 0 && iter->z / SQUARE_SIZE + 10 < heightZ) && (iter->x / SQUARE_SIZE - 10 >= 0 && iter->x / SQUARE_SIZE + 10 < widthX)) {
//				int idx = (int)(iter->z / SQUARE_SIZE) * widthX + (int)(iter->x / SQUARE_SIZE);
//				if (traversMap[idx] > 0.8) {
//					for (int hz = -10; hz <= 10; ++hz) {
//						for (int hx = -10; hx <= 10; ++hx) {
//							idx = (int)(iter->z / SQUARE_SIZE - hz) * widthX + iter->x / SQUARE_SIZE;
//							if (traversMap[idx] < 0.8) {
//								isKey = true;
//								break;
//							}
//						}
//						if (isKey) {
//							break;
//						}
//					}
//				}
//			}
//			if (!isKey) {
//				iter = points.erase(iter);
//			} else {
//				++iter;
//			}
//		}
//
////		Drawer* drawer = circuit->GetMap()->GetDrawer();
////		for (int i = 0; i < points.size(); i++) {
////			drawer->AddPoint(points[i], "");
////		}
////		delete drawer;
//
//		// step 4: Clusterize key waypoints
//		float maxDistance = circuit->GetUnitDefByName("corrl")->GetMaxWeaponRange() * 2;
//		maxDistance *= maxDistance;
//		circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>([circuit, points, maxDistance]() {
//			int nrows = points.size();
//			CRagMatrix distmatrix(nrows);
//			for (int i = 1; i < nrows; i++) {
//				for (int j = 0; j < i; j++) {
//					float dx = points[i].x - points[j].x;
//					float dz = points[i].z - points[j].z;
//					distmatrix(i, j) = dx * dx + dz * dz;
//				}
//			}
//
//			// Initialize cluster-element list
//			std::vector<std::vector<int>> iclusters;
//			iclusters.reserve(nrows);
//			for (int i = 0; i < nrows; i++) {
//				std::vector<int> cluster;
//				cluster.push_back(i);
//				iclusters.push_back(cluster);
//			}
//
//			for (int n = nrows; n > 1; n--) {
//				// Find pair
//				int is = 1;
//				int js = 0;
//				if (distmatrix.FindClosestPair(n, is, js) > maxDistance) {
//					break;
//				}
//
//				// Fix the distances
//				for (int j = 0; j < js; j++) {
//					distmatrix(js, j) = std::max(distmatrix(is, j), distmatrix(js, j));
//				}
//				for (int j = js + 1; j < is; j++) {
//					distmatrix(j, js) = std::max(distmatrix(is, j), distmatrix(j, js));
//				}
//				for (int j = is + 1; j < n; j++) {
//					distmatrix(j, js) = std::max(distmatrix(j, is), distmatrix(j, js));
//				}
//
//				for (int j = 0; j < is; j++) {
//					distmatrix(is, j) = distmatrix(n - 1, j);
//				}
//				for (int j = is + 1; j < n - 1; j++) {
//					distmatrix(j, is) = distmatrix(n - 1, j);
//				}
//
//				// Merge clusters
//				std::vector<int>& cluster = iclusters[js];
//				cluster.reserve(cluster.size() + iclusters[is].size()); // preallocate memory
//				cluster.insert(cluster.end(), iclusters[is].begin(), iclusters[is].end());
//				iclusters[is] = iclusters[n - 1];
//				iclusters.pop_back();
//			}
//
//			std::vector<std::vector<int>> clusters;
//			std::vector<AIFloat3> centroids;
//			int nclusters = iclusters.size();
//			clusters.resize(nclusters);
//			centroids.resize(nclusters);
//			for (int i = 0; i < nclusters; i++) {
//				clusters[i].clear();
//				AIFloat3 centr = ZeroVector;
//				for (int j = 0; j < iclusters[i].size(); j++) {
//					clusters[i].push_back(iclusters[i][j]);
//					centr += points[iclusters[i][j]];
//				}
//				centr /= iclusters[i].size();
//				centroids[i] = centr;
//			}
//
//			printf("nclusters: %i\n", nclusters);
//			for (int i = 0; i < clusters.size(); i++) {
//				printf("%i | ", clusters[i].size());
//			}
//			std::sort(clusters.begin(), clusters.end(), [](const std::vector<int>& a, const std::vector<int>& b){ return a.size() > b.size(); });
////			int num = std::min(10, (int)centroids.size());
//			int num = centroids.size();
//			Drawer* drawer = circuit->GetMap()->GetDrawer();
//			for (int i = 0; i < num; i++) {
//				AIFloat3 centr = ZeroVector;
//				for (int j = 0; j < clusters[i].size(); j++) {
//					centr += points[clusters[i][j]];
//				}
//				centr /= clusters[i].size();
//				drawer->AddPoint(centr, utils::string_format("%i", i).c_str());
//			}
//			delete drawer;
//		}));
//	}), FRAMES_PER_SEC);
}

CTerrainAnalyzer::~CTerrainAnalyzer()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

int CTerrainAnalyzer::GetTerrainWidth()
{
	return terrainWidth;
}

int CTerrainAnalyzer::GetTerrainHeight()
{
	return terrainHeight;
}

/*
 * FindBuildSiteSpace
 * @see rts/Game/GameHelper.cpp CGameHelper::ClosestBuildSite
 */
struct SearchOffset {
	int dx,dy;
	int qdist; // dx*dx+dy*dy
};
static bool SearchOffsetComparator (const SearchOffset& a, const SearchOffset& b)
{
	return a.qdist < b.qdist;
}
static const std::vector<SearchOffset>& GetSearchOffsetTable (int radius)
{
	static std::vector <SearchOffset> searchOffsets;
	unsigned int size = radius*radius*4;
	if (size > searchOffsets.size()) {
		searchOffsets.resize (size);

		for (int y = 0; y < radius*2; y++)
			for (int x = 0; x < radius*2; x++)
			{
				SearchOffset& i = searchOffsets[y*radius*2 + x];

				i.dx = x - radius;
				i.dy = y - radius;
				i.qdist = i.dx*i.dx + i.dy*i.dy;
			}

		std::sort(searchOffsets.begin(), searchOffsets.end(), SearchOffsetComparator);
	}

	return searchOffsets;
}
/*
 * const minDist = 4, using hax
 */
AIFloat3 CTerrainAnalyzer::FindBuildSiteSpace(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing)
{
	int xsize, zsize;
	switch (facing) {
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xsize = unitDef->GetZSize() * SQUARE_SIZE;
			zsize = unitDef->GetXSize() * SQUARE_SIZE;
			break;
		}
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH:
		default: {
			xsize = unitDef->GetXSize() * SQUARE_SIZE;
			zsize = unitDef->GetZSize() * SQUARE_SIZE;
			break;
		}
	}
	// HAX:  Use building as spacer because we don't have access to groundBlockingObjectMap.
	// TODO: Or maybe we can create own BlockingObjectMap as there is access to friendly units, features, map slopes.
	// TODO: Consider queued buildings
//	UnitDef* spacer4 = gameAttribute->GetUnitDefByName("striderhub");  // striderhub's size = 8 but can't recognize smooth hills
	UnitDef* spacer4 = circuit->GetUnitDefByName("armmstor");  // armmstor size = 6, thus we add diff (2) to pos when testing
	// spacer4->GetXSize() and spacer4->GetZSize() should be equal 6
	int size4 = spacer4->GetXSize();
//	assert(spacer4->GetXSize() == spacer4->GetZSize() && size4 == 6);
	int diff = (8 - size4) * SQUARE_SIZE;
	size4 *= SQUARE_SIZE;
	int xnum = xsize / size4 + 2;
	if (xnum % size4 == 0) {
		xnum--;  // check last cell manually for alignment purpose
	}
	int znum = zsize / size4 + 2;
	if (znum % size4 == 0) {
		znum--;  // check last cell manually for alignment purpose
	}
	UnitDef* mex = circuit->GetUnitDefByName("cormex");
	int xmsize = mex->GetXSize() * SQUARE_SIZE;
	int zmsize = mex->GetZSize() * SQUARE_SIZE;
	AIFloat3 spacerPos1(0, 0, 0), spacerPos2(0, 0, 0), probePos(0, 0, 0);

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const std::vector<SearchOffset>& ofs = GetSearchOffsetTable(endr);
	Map* map = circuit->GetMap();
	const float noffx = (xsize + size4) / 2;  // normal offset x
	const float noffz = (zsize + size4) / 2;  // normal offset z
	const float hoffx = noffx + diff;  // horizontal offset x
	const float voffz = noffz + diff;  // vertical offset z
	const float fsize4 = size4;
	const float moffx = (xsize + xmsize) / 2 + size4 + diff;  // mex offset x
	const float moffz = (zsize + xmsize) / 2 + size4 + diff;  // mex offset z
	CMetalManager* metalManager = circuit->GetMetalManager();
	for (int so = 0; so < endr * endr * 4; so++) {
		const float x = pos.x + ofs[so].dx * SQUARE_SIZE * 2;
		const float z = pos.z + ofs[so].dy * SQUARE_SIZE * 2;
		probePos.x = x;
		probePos.z = z;

		spacerPos1.x = probePos.x - moffx;
		spacerPos1.z = probePos.z - moffz;
		spacerPos2.x = probePos.x + moffx;
		spacerPos2.z = probePos.z + moffz;
		if (!metalManager->FindWithinRangeSpots(spacerPos1, spacerPos2).empty()) {
			continue;
		}

		if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
			bool good = true;
			// horizontal spacing
			spacerPos1.x = probePos.x - noffx;
			spacerPos1.z = probePos.z - voffz;
			spacerPos2.x = spacerPos1.x;
			spacerPos2.z = probePos.z + voffz;
			for (int ix = 0; ix < xnum; ix++) {
				if (!map->IsPossibleToBuildAt(spacer4, spacerPos1, 0) || !map->IsPossibleToBuildAt(spacer4, spacerPos2, 0)) {
					good = false;
					break;
				}
				spacerPos1.x += fsize4;
				spacerPos2.x = spacerPos1.x;
			}
			if (!good) {
				continue;
			}
			spacerPos1.x = probePos.x + noffx;
			spacerPos2.x = spacerPos1.x;
			if (!map->IsPossibleToBuildAt(spacer4, spacerPos1, 0) || !map->IsPossibleToBuildAt(spacer4, spacerPos2, 0)) {
				continue;
			}
			// vertical spacing
			spacerPos1.x = probePos.x - hoffx;
			spacerPos1.z = probePos.z - noffz;
			spacerPos2.x = probePos.x + hoffx;
			spacerPos2.z = spacerPos1.z;
			for (int iz = 0; iz < znum; iz++) {
				if (!map->IsPossibleToBuildAt(spacer4, spacerPos1, 0) || !map->IsPossibleToBuildAt(spacer4, spacerPos2, 0)) {
					good = false;
					break;
				}
				spacerPos1.z += fsize4;
				spacerPos2.z = spacerPos1.z;
			}
			if (!good) {
				continue;
			}
			spacerPos1.z = probePos.z + noffz;
			spacerPos2.z = spacerPos1.z;
			if (!map->IsPossibleToBuildAt(spacer4, spacerPos1, 0) || !map->IsPossibleToBuildAt(spacer4, spacerPos2, 0)) {
				continue;
			}
			if (good) {
				probePos.y = map->GetElevationAt(x, z);
				return probePos;
			}
		}
	}

	return -RgtVector;
}

AIFloat3 CTerrainAnalyzer::FindBuildSite(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing)
{
	int xsize, zsize;
	switch (facing) {
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xsize = unitDef->GetZSize() * SQUARE_SIZE;
			zsize = unitDef->GetXSize() * SQUARE_SIZE;
			break;
		}
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH:
		default: {
			xsize = unitDef->GetXSize() * SQUARE_SIZE;
			zsize = unitDef->GetZSize() * SQUARE_SIZE;
			break;
		}
	}
	UnitDef* mex = circuit->GetUnitDefByName("cormex");
	int xmsize = mex->GetXSize() * SQUARE_SIZE;
	int zmsize = mex->GetZSize() * SQUARE_SIZE;
	AIFloat3 spacerPos1(0, 0, 0), spacerPos2(0, 0, 0), probePos(0, 0, 0);

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const std::vector<SearchOffset>& ofs = GetSearchOffsetTable(endr);
	Map* map = circuit->GetMap();
	const float moffx = (xsize + xmsize) / 2 ;  // mex offset x
	const float moffz = (zsize + xmsize) / 2 ;  // mex offset z
	CMetalManager* metalManager = circuit->GetMetalManager();
	for (int so = 0; so < endr * endr * 4; so++) {
		const float x = pos.x + ofs[so].dx * SQUARE_SIZE * 2;
		const float z = pos.z + ofs[so].dy * SQUARE_SIZE * 2;
		probePos.x = x;
		probePos.z = z;

		spacerPos1.x = probePos.x - moffx;
		spacerPos1.z = probePos.z - moffz;
		spacerPos2.x = probePos.x + moffx;
		spacerPos2.z = probePos.z + moffz;
		if (!metalManager->FindWithinRangeSpots(spacerPos1, spacerPos2).empty()) {
			continue;
		}

		if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
			probePos.y = map->GetElevationAt(x, z);
			return probePos;
		}
	}

	return -RgtVector;
}

} // namespace circuit
