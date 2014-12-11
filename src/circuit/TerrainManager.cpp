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
#include "utils.h"

#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "OOAICallback.h"
#include "WeaponDef.h"

#include <algorithm>

namespace circuit {

using namespace springai;

#define MAX_BLOCK_VAL	0xFF
#define STRUCT			0x8000
#define GetBlock(x, z)	blockingMap[z * cellRows + x]

CTerrainManager::CTerrainManager(CCircuitAI* circuit) :
		IModule(circuit)
{
	Map* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	int mapHeight = map->GetHeight();
	terrainWidth = mapWidth * SQUARE_SIZE;
	terrainHeight = mapHeight * SQUARE_SIZE;

	cellRows = mapWidth / 2;  // build-step = 2 little green squares
	int cellsCount = cellRows * (mapHeight / 2);
	blockingMap.resize(cellsCount, 0);

	const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
	UnitDef* def = circuit->GetUnitDefByName("cormex");
	int size = std::max(def->GetXSize(), def->GetZSize()) / 2;
	int& xsize = size, &zsize = size;
	for (auto& spot : spots) {
		AIFloat3 pos = Pos2BuildPos(xsize, zsize, spot.position);
		const int x1 = int(pos.x / (SQUARE_SIZE << 1)) - (xsize >> 1), x2 = x1 + xsize;
		const int z1 = int(pos.z / (SQUARE_SIZE << 1)) - (zsize >> 1), z2 = z1 + zsize;
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap[z * cellRows + x] = MAX_BLOCK_VAL;
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

	def = circuit->GetUnitDefByName("factorycloak");
	BlockInfo info;
	info.xsize = def->GetXSize() / 2 + 6;
	info.zsize = def->GetZSize() / 2 + 3;
	info.offset = AIFloat3(0, 0, (SQUARE_SIZE * 2) * 4);
	blockInfo[def] = info;

	def = circuit->GetUnitDefByName("armfus");
	WeaponDef* wpDef;
	wpDef = circuit->GetCallback()->GetWeaponDefByName("atomic_blast");
	info.xsize = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	info.zsize = info.xsize;
	info.offset = ZeroVector;
	blockInfo[def] = info;

	def = circuit->GetUnitDefByName("cafus");
	wpDef = circuit->GetCallback()->GetWeaponDefByName("nuclear_missile");
	info.xsize = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	info.zsize = info.xsize;
	info.offset = ZeroVector;
	blockInfo[def] = info;

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

AIFloat3 CTerrainManager::Pos2BuildPos(int xsize, int zsize, const AIFloat3& pos)
{
	AIFloat3 buildPos;

	static const int HALFMAP_SQ = SQUARE_SIZE * 2;

	if (xsize & 1) {  // swaped Xsize, Zsize according to facing
		buildPos.x = floor((pos.x              ) / (HALFMAP_SQ)) * HALFMAP_SQ + SQUARE_SIZE;
	} else {
		buildPos.x = floor((pos.x + SQUARE_SIZE) / (HALFMAP_SQ)) * HALFMAP_SQ;
	}

	if (zsize & 1) {  // swaped Xsize, Zsize according to facing
		buildPos.z = floor((pos.z              ) / (HALFMAP_SQ)) * HALFMAP_SQ + SQUARE_SIZE;
	} else {
		buildPos.z = floor((pos.z + SQUARE_SIZE) / (HALFMAP_SQ)) * HALFMAP_SQ;
	}

//	pos.y = circuit->GetMap()->GetElevationAt(pos.x, pos.z);
	return pos;
}

const std::vector<CTerrainManager::SearchOffset>& CTerrainManager::GetSearchOffsetTable(int radius)
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

		auto searchOffsetComparator = [](const SearchOffset& a, const SearchOffset& b) {
			return a.qdist < b.qdist;
		};
		std::sort(searchOffsets.begin(), searchOffsets.end(), searchOffsetComparator);
	}

	return searchOffsets;
}

AIFloat3 CTerrainManager::FindBuildSite(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing)
{
	int xbsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	int zbsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	int xmsize, zmsize;
	AIFloat3 offset(ZeroVector);
	auto search = blockInfo.find(unitDef);
	if (search != blockInfo.end()) {
		BlockInfo& info = search->second;
		switch (facing) {
			default:
			case UNIT_FACING_SOUTH:
				xmsize = info.xsize;
				zmsize = info.zsize;
				offset.x = info.offset.x;
				offset.z = info.offset.z;
				break;
			case UNIT_FACING_EAST:
				xmsize = info.zsize;
				zmsize = info.xsize;
				offset.x = info.offset.z;
				offset.z = info.offset.x;
				break;
			case UNIT_FACING_NORTH:
				xmsize = info.xsize;
				zmsize = info.zsize;
				offset.x = info.offset.x;
				offset.z = -info.offset.z;
				break;
			case UNIT_FACING_WEST:
				xmsize = info.zsize;
				zmsize = info.xsize;
				offset.x = -info.offset.z;
				offset.z = info.offset.x;
				break;
		}
	} else {
		xmsize = xbsize;
		zmsize = zbsize;
	}

	// m - spacer mask; b - building
	auto isOpenSite = [this](int xm1, int xm2, int zm1, int zm2, int xb1, int xb2, int zb1, int zb2) {
		for (int x = xm1; x < xm2; x++) {
			for (int z = zm1; z < zb1; z++) {
				if (GetBlock(x, z) & STRUCT) {
					return false;
				}
			}
			for (int z = zb2; z < zm2; z++) {
				if (GetBlock(x, z) & STRUCT) {
					return false;
				}
			}
		}
		for (int z = zb1; z < zb2; z++) {
			for (int x = xm1; x < xb1; x++) {
				if (GetBlock(x, z) & STRUCT) {
					return false;
				}
			}
			for (int x = xb2; x < xm2; x++) {
				if (GetBlock(x, z) & STRUCT) {
					return false;
				}
			}
			for (int x = xb1; x < xb2; x++) {
				if (GetBlock(x, z) > 0) {
					return false;
				}
			}
		}
		return true;
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const std::vector<SearchOffset>& ofs = GetSearchOffsetTable(endr);
	Map* map = circuit->GetMap();
	AIFloat3 buildPos = Pos2BuildPos(xmsize, zmsize, pos);
	const int cornerX1 = int(buildPos.x / (SQUARE_SIZE * 2)) - (xbsize / 2);
	const int cornerZ1 = int(buildPos.z / (SQUARE_SIZE * 2)) - (zbsize / 2);
	AIFloat3 blockPos = buildPos + offset;
	const int maskCornerX1 = int(blockPos.x / (SQUARE_SIZE * 2)) - (xmsize / 2);
	const int maskCornerZ1 = int(blockPos.z / (SQUARE_SIZE * 2)) - (zmsize / 2);
	AIFloat3 probePos(ZeroVector);
	for (int so = 0; so < endr * endr * 4; so++) {
		int xm1 = maskCornerX1 + ofs[so].dx, xm2 = xm1 + xmsize;
		int zm1 = maskCornerZ1 + ofs[so].dy, zm2 = zm1 + zmsize;
		int xb1 = cornerX1 + ofs[so].dx, xb2 = xb1 + xbsize;
		int zb1 = cornerZ1 + ofs[so].dy, zb2 = zb1 + zbsize;
		if (!isOpenSite(xm1, xm2, zm1, zm2, xb1, xb2, zb1, zb2)) {
			continue;
		}

		probePos.x = (xb1 + xb2) * SQUARE_SIZE;
		probePos.z = (zb1 + zb2) * SQUARE_SIZE;
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

	int xbsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	int zbsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	int xmsize, zmsize;
	AIFloat3 offset(ZeroVector);
	auto search = blockInfo.find(unitDef);
	if (search != blockInfo.end()) {
		BlockInfo& info = search->second;
		switch (facing) {
			default:
			case UNIT_FACING_SOUTH:
				xmsize = info.xsize;
				zmsize = info.zsize;
				offset.x = info.offset.x;
				offset.z = info.offset.z;
				break;
			case UNIT_FACING_EAST:
				xmsize = info.zsize;
				zmsize = info.xsize;
				offset.x = info.offset.z;
				offset.z = info.offset.x;
				break;
			case UNIT_FACING_NORTH:
				xmsize = info.xsize;
				zmsize = info.zsize;
				offset.x = info.offset.x;
				offset.z = -info.offset.z;
				break;
			case UNIT_FACING_WEST:
				xmsize = info.zsize;
				zmsize = info.xsize;
				offset.x = -info.offset.z;
				offset.z = info.offset.x;
				break;
		}
	} else {
		xmsize = xbsize;
		zmsize = zbsize;
	}

	/*
	 * xm1     xb1   xb2     xm2
	 * |       |     |       |
	 * mmmmmmmmmmmmmmmmmmmmmm - zm1
	 * mmmmmmmmmmmmmmmmmmmmmm
	 * mmmmmmmmbbbbbbmmmmmmmm - zb1
	 * mmmmmmmmbbbbbbmmmmmmmm
	 * mmmmmmmmbbbbbbmmmmmmmm
	 * mmmmmmmmmmmmmmmmmmmmmm - zb2
	 * mmmmmmmmmmmmmmmmmmmmmm
	 *                        - zm2
	 */

	AIFloat3 buildPos = Pos2BuildPos(xmsize, zmsize, u->GetPos());
	const int xb1 = int(buildPos.x / (SQUARE_SIZE * 2)) - (xbsize / 2), xb2 = xb1 + xbsize;
	const int zb1 = int(buildPos.z / (SQUARE_SIZE * 2)) - (zbsize / 2), zb2 = zb1 + zbsize;
	AIFloat3 blockPos = buildPos + offset;
	const int xm1 = int(blockPos.x / (SQUARE_SIZE * 2)) - (xmsize / 2), xm2 = xm1 + xmsize;
	const int zm1 = int(blockPos.z / (SQUARE_SIZE * 2)) - (zmsize / 2), zm2 = zm1 + zmsize;
	for (int x = xm1; x < xm2; x++) {
		for (int z = zm1; z < zb1; z++) {
			GetBlock(x, z) += count;
		}
		for (int z = zb2; z < zm2; z++) {
			GetBlock(x, z) += count;
		}
	}
	for (int z = zb1; z < zb2; z++) {
		for (int x = xm1; x < xb1; x++) {
			GetBlock(x, z) += count;
		}
		for (int x = xb2; x < xm2; x++) {
			GetBlock(x, z) += count;
		}
		if (count > 0) {
			for (int x = xb1; x < xb2; x++) {
				GetBlock(x, z) |= STRUCT;
			}
		} else {
			// NOTE: This can be wrong if unit was built inside factory :/
			// FIX: Do not mark movable units
			for (int x = xb1; x < xb2; x++) {
				GetBlock(x, z) &= ~STRUCT;
			}
		}
	}
}

} // namespace circuit
