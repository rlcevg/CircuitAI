/*
 * TerrainManager.cpp
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#include "CircuitAI.h"
#include "CircuitUnit.h"
#include "MetalManager.h"
#include "TerrainManager.h"
#include "BlockRectangle.h"
#include "BlockCircle.h"
#include "utils.h"

#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "OOAICallback.h"
#include "WeaponDef.h"

// debug
#include "Drawer.h"
#include "Scheduler.h"

#define MAX_BLOCK_VAL	0xFF
#define STRUCTURE		0x8000
#define GRID_RATIO_LOW	8

namespace circuit {

using namespace springai;

inline bool CTerrainManager::BlockingMap::IsStruct(int x, int z)
{
	return (grid[z * columns + x] & STRUCTURE);
}

inline bool CTerrainManager::BlockingMap::IsBlocked(int x, int z)
{
	return (grid[z * columns + x] > 0);
}

inline bool CTerrainManager::BlockingMap::IsBlockedLow(int x, int z)
{
	return (gridLow[z * columnsLow + x] >= (GRID_RATIO_LOW - 1) * (GRID_RATIO_LOW - 1));
}

inline void CTerrainManager::BlockingMap::MarkBlocker(int x, int z)
{
	grid[z * columns + x] = MAX_BLOCK_VAL;
	gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]++;
}

inline void CTerrainManager::BlockingMap::AddBlocker(int x, int z)
{
	int index = z * columns + x;
	if (grid[index] == 0) {
		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]++;
	}
	grid[index]++;
}

inline void CTerrainManager::BlockingMap::RemoveBlocker(int x, int z)
{
	int index = z * columns + x;
	grid[index]--;
	if (grid[index] == 0) {
		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]--;
	}
}

inline void CTerrainManager::BlockingMap::AddStruct(int x, int z)
{
	int index = z * columns + x;
	if (grid[index] == 0) {
		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]++;
	}
	grid[index] |= STRUCTURE;
}

inline void CTerrainManager::BlockingMap::RemoveStruct(int x, int z)
{
	int index = z * columns + x;
	grid[index] &= ~STRUCTURE;
	if (grid[index] == 0) {
		gridLow[z / GRID_RATIO_LOW * columnsLow + x / GRID_RATIO_LOW]--;
	}
}

CTerrainManager::CTerrainManager(CCircuitAI* circuit) :
		IModule(circuit)
{
	Map* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	int mapHeight = map->GetHeight();
	terrainWidth = mapWidth * SQUARE_SIZE;
	terrainHeight = mapHeight * SQUARE_SIZE;

	int cellsRow = mapWidth / 2;  // build-step = 2 little green squares
	blockingMap.columns = cellsRow;
	blockingMap.grid.resize(cellsRow * (mapHeight / 2), 0);
	int cellsRowLow = mapWidth / (GRID_RATIO_LOW * 2);
	blockingMap.columnsLow = cellsRowLow;
	blockingMap.gridLow.resize(cellsRowLow * (mapHeight / (GRID_RATIO_LOW * 2)), 0);

	const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
	UnitDef* def = circuit->GetUnitDefByName("cormex");
	int size = std::max(def->GetXSize(), def->GetZSize()) / 2;
	int& xsize = size, &zsize = size;
	for (auto& spot : spots) {
		const int x1 = int(spot.position.x / (SQUARE_SIZE << 1)) - (xsize >> 1), x2 = x1 + xsize;
		const int z1 = int(spot.position.z / (SQUARE_SIZE << 1)) - (zsize >> 1), z2 = z1 + zsize;
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.MarkBlocker(x, z);
			}
		}
	}

	/*
	 * building handlers
	 */
	auto buildingCreatedHandler = [this](CCircuitUnit* unit) {
		AddBlocker(unit);
	};
	auto buildingDestroyedHandler = [this](CCircuitUnit* unit) {
		RemoveBlocker(unit);
	};

	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
	for (auto& kv : defs) {
		UnitDef* def = kv.second;
		int unitDefId = def->GetUnitDefId();
		if (def->GetSpeed() == 0) {
			createdHandler[unitDefId] = buildingCreatedHandler;
			destroyedHandler[unitDefId] = buildingDestroyedHandler;
		}
	}

	/*
	 * building masks
	 */
//	def = circuit->GetUnitDefByName("armsolar");
//	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
//	auto search = customParams.find("pylonrange");
//	if (search != customParams.end()) {
//		float radius = utils::string_to_float(search->second);
//		BlockInfo info;
//		info.xsize = def->GetXSize() / 2;
//		info.zsize = def->GetZSize() / 2;
//		info.offset = ZeroVector;
//		blockInfo[def] = info;
//	}

	WeaponDef* wpDef;
	int2 offset;
	int2 bsize;
	int2 ssize;
	int radius;
	def = circuit->GetUnitDefByName("factorycloak");
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize + int2(6, 4);
	// offset in South facing
	offset = int2(0, 4);
	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize);

	def = circuit->GetUnitDefByName("armfus");
	wpDef = circuit->GetCallback()->GetWeaponDefByName("atomic_blast");
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	blockInfos[def] = new CBlockCircle(offset, radius, ssize);

	def = circuit->GetUnitDefByName("cafus");
	wpDef = circuit->GetCallback()->GetWeaponDefByName("nuclear_missile");
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	blockInfos[def] = new CBlockCircle(offset, radius, ssize);

//	def = circuit->GetUnitDefByName("cormex");
//	ssize = int2(0, 0);
//	bsize = ssize;
//	offset = ssize;
//	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize);

	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>([this]() {
		Drawer* drawer = this->circuit->GetMap()->GetDrawer();
		for (int z = 0; z < this->circuit->GetMap()->GetWidth() / 16; z++) {
			for (int x = 0; x < blockingMap.columnsLow; x++) {
				AIFloat3 ppp(x * GRID_RATIO_LOW * SQUARE_SIZE * 2 + GRID_RATIO_LOW * SQUARE_SIZE, 0 , z * GRID_RATIO_LOW * SQUARE_SIZE * 2 + GRID_RATIO_LOW * SQUARE_SIZE);
				drawer->DeletePointsAndLines(ppp);
			}
		}
		delete drawer;
	}), FRAMES_PER_SEC * 60);
	circuit->GetScheduler()->RunTaskEvery(std::make_shared<CGameTask>([this]() {
		Drawer* drawer = this->circuit->GetMap()->GetDrawer();
		for (int z = 0; z < this->circuit->GetMap()->GetWidth() / 16; z++) {
			for (int x = 0; x < blockingMap.columnsLow; x++) {
				if (blockingMap.IsBlockedLow(x, z)) {
					AIFloat3 ppp(x * GRID_RATIO_LOW * SQUARE_SIZE * 2 + GRID_RATIO_LOW * SQUARE_SIZE, 0 , z * GRID_RATIO_LOW * SQUARE_SIZE * 2 + GRID_RATIO_LOW * SQUARE_SIZE);
					drawer->AddPoint(ppp, "");
				}
			}
		}
		delete drawer;
	}), FRAMES_PER_SEC * 60, FRAMES_PER_SEC * 5);


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
//		const std::vector<float>& heightMap = circuit->GetMap()->GetHeightMap();
//		const std::vector<float>& slopeMap = circuit->GetMap()->GetSlopeMap();
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

CTerrainManager::~CTerrainManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& kv : blockInfos) {
		delete kv.second;
	}
}

int CTerrainManager::UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder)
{
	auto search = createdHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != createdHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CTerrainManager::UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker)
{
	auto search = destroyedHandler.find(unit->GetDef()->GetUnitDefId());
	if (search != destroyedHandler.end()) {
		search->second(unit);
	}

	return 0; //signaling: OK
}

int CTerrainManager::GetTerrainWidth()
{
	return terrainWidth;
}

int CTerrainManager::GetTerrainHeight()
{
	return terrainHeight;
}

AIFloat3 CTerrainManager::FindBuildSite(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing)
{
	auto search = blockInfos.find(unitDef);
	if (search != blockInfos.end()) {
		return FindBuildSiteByMask(unitDef, pos, searchRadius, facing, search->second);
	}

	if (searchRadius > SQUARE_SIZE * 2 * 100) {
		return FindBuildSiteLow(unitDef, pos, searchRadius, facing);
	}

	/*
	 * Default FindBuildSite
	 */
	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	auto isOpenSite = [this](int x1, int x2, int z1, int z2) {
		for (int x = x1; x < x2; x++) {
			for (int z = z1; z < z2; z++) {
				if (blockingMap.IsBlocked(x, z)) {
					return false;
				}
			}
		}
		return true;
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const std::vector<SearchOffset>& ofs = GetSearchOffsetTable(endr);
	const int cornerX1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2);
	const int cornerZ1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2);
	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();
	for (int so = 0; so < endr * endr * 4; so++) {
		int x1 = cornerX1 + ofs[so].dx, x2 = x1 + xsize;
		int z1 = cornerZ1 + ofs[so].dy, z2 = z1 + zsize;
		if (!isOpenSite(x1, x2, z1, z2)) {
			continue;
		}

		probePos.x = (x1 + x2) * SQUARE_SIZE;
		probePos.z = (z1 + z2) * SQUARE_SIZE;
		if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
			probePos.y = map->GetElevationAt(probePos.x, probePos.z);
			return probePos;
		}
	}

	return -RgtVector;
}

//AIFloat3 CTerrainManager::Pos2BuildPos(int xsize, int zsize, const AIFloat3& pos)
//{
//	AIFloat3 buildPos;
//
//	static const int HALFMAP_SQ = SQUARE_SIZE * 2;
//
//	if (xsize & 1) {  // swaped Xsize, Zsize according to facing
//		buildPos.x = floor((pos.x              ) / (HALFMAP_SQ)) * HALFMAP_SQ + SQUARE_SIZE;
//	} else {
//		buildPos.x = floor((pos.x + SQUARE_SIZE) / (HALFMAP_SQ)) * HALFMAP_SQ;
//	}
//
//	if (zsize & 1) {  // swaped Xsize, Zsize according to facing
//		buildPos.z = floor((pos.z              ) / (HALFMAP_SQ)) * HALFMAP_SQ + SQUARE_SIZE;
//	} else {
//		buildPos.z = floor((pos.z + SQUARE_SIZE) / (HALFMAP_SQ)) * HALFMAP_SQ;
//	}
//
////	buildPos.y = circuit->GetMap()->GetElevationAt(buildPos.x, buildPos.z);
//	buildPos.y = pos.y;
//	return buildPos;
//}

const CTerrainManager::SearchOffsets& CTerrainManager::GetSearchOffsetTable(int radius)
{
	static std::vector<SearchOffset> searchOffsets;
	unsigned int size = radius * radius * 4;
	if (size > searchOffsets.size()) {
		searchOffsets.resize(size);

		for (int y = 0; y < radius * 2; y++) {
			for (int x = 0; x < radius * 2; x++) {
				SearchOffset& i = searchOffsets[y * radius * 2 + x];

				i.dx = x - radius;
				i.dy = y - radius;
				i.qdist = i.dx * i.dx + i.dy * i.dy;
			}
		}

		auto searchOffsetComparator = [](const SearchOffset& a, const SearchOffset& b) {
			return a.qdist < b.qdist;
		};
		std::sort(searchOffsets.begin(), searchOffsets.end(), searchOffsetComparator);
	}

	return searchOffsets;
}

const CTerrainManager::SearchOffsetsLow& CTerrainManager::GetSearchOffsetTableLow(int radius)
{
	static SearchOffsetsLow searchOffsetsLow;
	int radiusLow = radius / GRID_RATIO_LOW;
	unsigned int sizeLow = radiusLow * radiusLow * 4;
	if (sizeLow > searchOffsetsLow.size()) {
		searchOffsetsLow.resize(sizeLow);

		SearchOffsets searchOffsets;
		searchOffsets.resize(radius * radius * 4);
		for (int y = 0; y < radius * 2; y++) {
			for (int x = 0; x < radius * 2; x++) {
				SearchOffset& i = searchOffsets[y * radius * 2 + x];
				i.dx = x - radius;
				i.dy = y - radius;
				i.qdist = i.dx * i.dx + i.dy * i.dy;
			}
		}

		auto searchOffsetComparator = [](const SearchOffset& a, const SearchOffset& b) {
			return a.qdist < b.qdist;
		};
		for (int yl = 0; yl < radiusLow * 2; yl++) {
			for (int xl = 0; xl < radiusLow * 2; xl++) {
				SearchOffsetLow& il = searchOffsetsLow[yl * radiusLow * 2 + xl];
				il.dx = xl - radiusLow;
				il.dy = yl - radiusLow;
				il.qdist = il.dx * il.dx + il.dy * il.dy;

				il.ofs.reserve(GRID_RATIO_LOW * GRID_RATIO_LOW);
				const int xi = xl * GRID_RATIO_LOW;
				for (int y = yl * GRID_RATIO_LOW; y < (yl + 1) * GRID_RATIO_LOW; y++) {
					for (int x = xi; x < xi + GRID_RATIO_LOW; x++) {
						il.ofs.push_back(searchOffsets[y * radius * 2 + x]);
					}
				}

				std::sort(il.ofs.begin(), il.ofs.end(), searchOffsetComparator);
			}
		}

		auto searchOffsetLowComparator = [](const SearchOffsetLow& a, const SearchOffsetLow& b) {
			return a.qdist < b.qdist;
		};
		std::sort(searchOffsetsLow.begin(), searchOffsetsLow.end(), searchOffsetLowComparator);
	}

	return searchOffsetsLow;
}

AIFloat3 CTerrainManager::FindBuildSiteLow(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing)
{
	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	auto isOpenSite = [this](int x1, int x2, int z1, int z2) {
		for (int x = x1; x < x2; x++) {
			for (int z = z1; z < z2; z++) {
				if (blockingMap.IsBlocked(x, z)) {
					return false;
				}
			}
		}
		return true;
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const SearchOffsetsLow& ofsLow = GetSearchOffsetTableLow(endr);
	const int endrLow = endr / GRID_RATIO_LOW;
	const int centerX = int(pos.x / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));
	const int centerZ = int(pos.z / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));
	const int cornerX1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2);
	const int cornerZ1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2);
	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();
	for (int soLow = 0; soLow < endrLow * endrLow * 4; soLow++) {
		int xlow = centerX + ofsLow[soLow].dx;
		int zlow = centerZ + ofsLow[soLow].dy;
		if (blockingMap.IsBlockedLow(xlow, zlow)) {
			continue;
		}

		const SearchOffsets& ofs = ofsLow[soLow].ofs;
		for (int so = 0; so < GRID_RATIO_LOW * GRID_RATIO_LOW; so++) {
			int x1 = cornerX1 + ofs[so].dx, x2 = x1 + xsize;
			int z1 = cornerZ1 + ofs[so].dy, z2 = z1 + zsize;
			if (!isOpenSite(x1, x2, z1, z2)) {
				continue;
			}

			probePos.x = (x1 + x2) * SQUARE_SIZE;
			probePos.z = (z1 + z2) * SQUARE_SIZE;
			if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
				probePos.y = map->GetElevationAt(probePos.x, probePos.z);
				return probePos;
			}
		}
	}

	return -RgtVector;
}

AIFloat3 CTerrainManager::FindBuildSiteByMask(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask)
{
	int xmsize = mask->GetXSize();
	int zmsize = mask->GetZSize();
	printf("lowSearch: %i\n", (searchRadius > SQUARE_SIZE * 2 * 100) || (xmsize * zmsize > GRID_RATIO_LOW * GRID_RATIO_LOW * 9));
	if ((searchRadius > SQUARE_SIZE * 2 * 100) || (xmsize * zmsize > GRID_RATIO_LOW * GRID_RATIO_LOW * 9)) {
		return FindBuildSiteByMaskLow(unitDef, pos, searchRadius, facing, mask);
	}

	int xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
			break;
		}
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
			break;
		}
	}

#define DECLARE_TEST(testName, facingType)								\
	auto testName = [this, mask](const int2& m1, const int2& m2) {		\
		for (int x = m1.x, xm = 0; x < m2.x; x++, xm++) {				\
			for (int z = m1.y, zm = 0; z < m2.y; z++, zm++) {			\
				switch (mask->facingType(xm, zm)) {						\
					case IBlockMask::BlockType::BLOCKED: {				\
						if (blockingMap.IsStruct(x, z)) {				\
							return false;								\
						}												\
						break;											\
					}													\
					case IBlockMask::BlockType::STRUCT: {				\
						if (blockingMap.IsBlocked(x, z)) {				\
							return false;								\
						}												\
						break;											\
					}													\
				}														\
			}															\
		}																\
		return true;													\
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const SearchOffsets& ofs = GetSearchOffsetTable(endr);
	int2 corner;
	corner.x = int(pos.x / (SQUARE_SIZE * 2)) - (xssize / 2);
	corner.y = int(pos.z / (SQUARE_SIZE * 2)) - (zssize / 2);
	const int2& offset = mask->GetStructOffset(facing);
	int2 maskCorner = corner - offset;
	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();

#define DO_TEST(testName)												\
	for (int so = 0; so < endr * endr * 4; so++) {						\
		int2 m1(maskCorner.x + ofs[so].dx, maskCorner.y + ofs[so].dy);	\
		int2 m2(        m1.x + xmsize,             m1.y + zmsize);		\
		if (!testName(m1, m2)) {										\
			continue;													\
		}																\
																		\
		int2 b1(    corner.x + ofs[so].dx,     corner.y + ofs[so].dy);	\
		int2 b2(        b1.x + xssize,             b1.y + zssize);		\
		probePos.x = (b1.x + b2.x) * SQUARE_SIZE;						\
		probePos.z = (b1.y + b2.y) * SQUARE_SIZE;						\
		if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {		\
			probePos.y = map->GetElevationAt(probePos.x, probePos.z);	\
			return probePos;											\
		}																\
	}

	switch (facing) {
		case UNIT_FACING_SOUTH: {
			DECLARE_TEST(isOpenSouth, GetTypeSouth);
			DO_TEST(isOpenSouth);
			break;
		}
		case UNIT_FACING_EAST: {
			DECLARE_TEST(isOpenEast, GetTypeEast);
			DO_TEST(isOpenEast);
			break;
		}
		case UNIT_FACING_NORTH: {
			DECLARE_TEST(isOpenNorth, GetTypeNorth);
			DO_TEST(isOpenNorth);
			break;
		}
		case UNIT_FACING_WEST: {
			DECLARE_TEST(isOpenWest, GetTypeWest);
			DO_TEST(isOpenWest);
			break;
		}
	}

	return -RgtVector;
}

AIFloat3 CTerrainManager::FindBuildSiteByMaskLow(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask)
{
	int xmsize, zmsize, xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xmsize = mask->GetXSize();
			zmsize = mask->GetZSize();
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
			break;
		}
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
			break;
		}
	}

#define DECLARE_TEST_LOW(testName, facingType)								\
	auto testName = [this, mask](const int2& m1, const int2& m2) {			\
		for (int x = m1.x, xm = 0; x < m2.x; x++, xm++) {					\
			for (int z = m1.y, zm = 0; z < m2.y; z++, zm++) {				\
				switch (mask->facingType(xm, zm)) {							\
					case IBlockMask::BlockType::BLOCKED: {					\
						if (blockingMap.IsStruct(x, z)) {					\
							return false;									\
						}													\
						break;												\
					}														\
					case IBlockMask::BlockType::STRUCT: {					\
						if (blockingMap.IsBlocked(x, z)) {					\
							return false;									\
						}													\
						break;												\
					}														\
				}															\
			}																\
		}																	\
		return true;														\
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const SearchOffsetsLow& ofsLow = GetSearchOffsetTableLow(endr);
	const int endrLow = endr / GRID_RATIO_LOW;
	int2 corner;
	corner.x = int(pos.x / (SQUARE_SIZE * 2)) - (xssize / 2);
	corner.y = int(pos.z / (SQUARE_SIZE * 2)) - (zssize / 2);
	const int2& offset = mask->GetStructOffset(facing);
	int2 maskCorner = corner - offset;
	int2 center;
	center.x = int(pos.x / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));
	center.y = int(pos.x / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));
	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();

#define DO_TEST_LOW(testName)												\
	for (int soLow = 0; soLow < endrLow * endrLow * 4; soLow++) {			\
		int2 low(center.x + ofsLow[soLow].dx, center.y + ofsLow[soLow].dy);	\
		if (blockingMap.IsBlockedLow(low.x, low.y)) {						\
			continue;														\
		}																	\
																			\
		const SearchOffsets& ofs = ofsLow[soLow].ofs;						\
		for (int so = 0; so < GRID_RATIO_LOW * GRID_RATIO_LOW; so++) {				\
			int2 m1(maskCorner.x + ofs[so].dx, maskCorner.y + ofs[so].dy);	\
			int2 m2(        m1.x + xmsize,             m1.y + zmsize);		\
			if (!testName(m1, m2)) {										\
				continue;													\
			}																\
																			\
			int2 b1(    corner.x + ofs[so].dx,     corner.y + ofs[so].dy);	\
			int2 b2(        b1.x + xssize,             b1.y + zssize);		\
			probePos.x = (b1.x + b2.x) * SQUARE_SIZE;						\
			probePos.z = (b1.y + b2.y) * SQUARE_SIZE;						\
			if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {		\
				probePos.y = map->GetElevationAt(probePos.x, probePos.z);	\
				return probePos;											\
			}																\
		}																	\
	}

	switch (facing) {
		case UNIT_FACING_SOUTH: {
			DECLARE_TEST_LOW(isOpenSouth, GetTypeSouth);
			DO_TEST_LOW(isOpenSouth);
			break;
		}
		case UNIT_FACING_EAST: {
			DECLARE_TEST_LOW(isOpenEast, GetTypeEast);
			DO_TEST_LOW(isOpenEast);
			break;
		}
		case UNIT_FACING_NORTH: {
			DECLARE_TEST_LOW(isOpenNorth, GetTypeNorth);
			DO_TEST_LOW(isOpenNorth);
			break;
		}
		case UNIT_FACING_WEST: {
			DECLARE_TEST_LOW(isOpenWest, GetTypeWest);
			DO_TEST_LOW(isOpenWest);
			break;
		}
	}

	return -RgtVector;
}

void CTerrainManager::AddBlocker(CCircuitUnit* unit)
{
	MarkBlocker(unit, true);
}

void CTerrainManager::RemoveBlocker(CCircuitUnit* unit)
{
	MarkBlocker(unit, false);
}

void CTerrainManager::MarkBlockerByMask(CCircuitUnit* unit, bool block, IBlockMask* mask)
{
	Unit* u = unit->GetUnit();
	UnitDef* unitDef = unit->GetDef();
	int facing = u->GetBuildingFacing();
	const AIFloat3& pos = u->GetPos();

	int xmsize, zmsize, xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xmsize = mask->GetXSize();
			zmsize = mask->GetZSize();
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
			break;
		}
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
			break;
		}
	}

#define DECLARE_MARKER(typeName, blockerOp, structOp)				\
	for (int x = m1.x, xm = 0; x < m2.x; x++, xm++) {				\
		for (int z = m1.y, zm = 0; z < m2.y; z++, zm++) {			\
			switch (mask->typeName(xm, zm)) {						\
				case IBlockMask::BlockType::BLOCKED: {				\
					blockingMap.blockerOp(x, z);					\
					break;											\
				}													\
				case IBlockMask::BlockType::STRUCT: {				\
					blockingMap.structOp(x, z);						\
					break;											\
				}													\
			}														\
		}															\
	}

	int2 corner;
	corner.x = int(pos.x / (SQUARE_SIZE * 2)) - (xssize / 2);
	corner.y = int(pos.z / (SQUARE_SIZE * 2)) - (zssize / 2);
	int2 m1 = corner - mask->GetStructOffset(facing);
	int2 m2(m1.x + xmsize, m1.y + zmsize);
	Map* map = circuit->GetMap();

#define DO_MARK(facingType)											\
	if (block) {													\
		DECLARE_MARKER(facingType, AddBlocker, AddStruct);			\
	} else {														\
		DECLARE_MARKER(facingType, RemoveBlocker, RemoveStruct);	\
	}

	switch (facing) {
		case UNIT_FACING_SOUTH: {
			DO_MARK(GetTypeSouth);
			break;
		}
		case UNIT_FACING_EAST: {
			DO_MARK(GetTypeEast);
			break;
		}
		case UNIT_FACING_NORTH: {
			DO_MARK(GetTypeNorth);
			break;
		}
		case UNIT_FACING_WEST: {
			DO_MARK(GetTypeWest);
			break;
		}
	}
}

void CTerrainManager::MarkBlocker(CCircuitUnit* unit, bool block)
{
	UnitDef* unitDef = unit->GetDef();
	auto search = blockInfos.find(unitDef);
	if (search != blockInfos.end()) {
		MarkBlockerByMask(unit, block, search->second);
		return;
	}

	/*
	 * Default marker
	 */
	Unit* u = unit->GetUnit();
	int facing = u->GetBuildingFacing();
	const AIFloat3& pos = u->GetPos();

	int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	const int x1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2), x2 = x1 + xsize;
	const int z1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2), z2 = z1 + zsize;
	if (block) {
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.AddStruct(x, z);
			}
		}
	} else {
		// NOTE: This can mess up things if unit is inside factory :/
		// SOLUTION: Do not mark movable units
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.RemoveStruct(x, z);
			}
		}
	}
}

} // namespace circuit
