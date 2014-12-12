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

namespace circuit {

using namespace springai;

CTerrainManager::CTerrainManager(CCircuitAI* circuit) :
		IModule(circuit)
{
	Map* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	int mapHeight = map->GetHeight();
	terrainWidth = mapWidth * SQUARE_SIZE;
	terrainHeight = mapHeight * SQUARE_SIZE;

	int cellRows = mapWidth / 2;  // build-step = 2 little green squares
	int cellsCount = cellRows * (mapHeight / 2);
	blockingMap.cellRows = cellRows;
	blockingMap.grid.resize(cellsCount, 0);

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

	// debug
	Drawer* drawer = circuit->GetMap()->GetDrawer();
	for (int z = 0; z < cellRows; z++) {
		for (int x = 0; x < mapHeight / 2; x++) {
			if (blockingMap.IsBlocked(x, z)) {
				AIFloat3 pos = AIFloat3(x * SQUARE_SIZE * 2 + SQUARE_SIZE, 0, z * SQUARE_SIZE * 2 + SQUARE_SIZE);
				drawer->AddPoint(pos, "");
			}
		}
	}
	delete drawer;

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
	AIFloat3 offset;
	int radius;
	def = circuit->GetUnitDefByName("factorycloak");
	xsize = def->GetXSize() / 2 + 6;
	zsize = def->GetZSize() / 2 + 4;
	offset = AIFloat3(0, 0, (SQUARE_SIZE * 2) * 4);
	blockInfos[def] = new CBlockRectangle(offset, xsize, zsize);

	def = circuit->GetUnitDefByName("armfus");
	wpDef = circuit->GetCallback()->GetWeaponDefByName("atomic_blast");
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	offset = ZeroVector;
	blockInfos[def] = new CBlockCircle(offset, radius);

	def = circuit->GetUnitDefByName("cafus");
	wpDef = circuit->GetCallback()->GetWeaponDefByName("nuclear_missile");
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	offset = ZeroVector;
	blockInfos[def] = new CBlockCircle(offset, radius);

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
		return search->second->FindBuildSite(blockingMap, unitDef, facing, pos, searchRadius, circuit->GetMap());
	}

	/*
	 * Default FindBuildSite
	 */
	int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	// m - spacer mask; b - building
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
	const std::vector<IBlockInfo::SearchOffset>& ofs = IBlockInfo::GetSearchOffsetTable(endr);
	Map* map = circuit->GetMap();
	const int cornerX1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2);
	const int cornerZ1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2);
	AIFloat3 probePos(ZeroVector);
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

void CTerrainManager::AddBlocker(CCircuitUnit* unit)
{
	MarkBlocker(unit, 1);
}

void CTerrainManager::RemoveBlocker(CCircuitUnit* unit)
{
	MarkBlocker(unit, -1);
}

void CTerrainManager::MarkBlocker(CCircuitUnit* unit, int count)
{
	Unit* u = unit->GetUnit();
	UnitDef* unitDef = unit->GetDef();
	int facing = u->GetBuildingFacing();
	const AIFloat3& pos = u->GetPos();

	int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	auto search = blockInfos.find(unitDef);
	if (search != blockInfos.end()) {
		search->second->MarkBlocker(blockingMap, facing, xsize, zsize, pos, count);
		return;
	}

	const int x1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2), x2 = x1 + xsize;
	const int z1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2), z2 = z1 + zsize;
	if (count > 0) {
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.AddStruct(x, z);
			}
		}
	} else {
		// NOTE: This can be wrong if unit was built inside factory :/
		// SOLUTION: Do not mark movable units
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.RemoveStruct(x, z);
			}
		}
	}
}

} // namespace circuit
