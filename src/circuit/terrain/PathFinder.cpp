/*
 * PathFinder.cpp
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/PathFinder.cpp
 */

#include "terrain/PathFinder.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainManager.h"
#include "map/ThreatMap.h"
#include "unit/CircuitUnit.h"
#include "util/utils.h"
#ifdef DEBUG_VIS
#include "CircuitAI.h"
#endif

#include "spring/SpringMap.h"

#ifdef DEBUG_VIS
#include "Figure.h"
#endif

namespace circuit {

using namespace springai;
using namespace NSMicroPather;

#define THREAT_EPSILON		1e-2f
#define MOVE_EPSILON		1e-1f
#define SPIDER_SLOPE		0.99f

std::vector<int> CPathFinder::blockArray;

CPathFinder::CPathFinder(CTerrainData* terrainData)
		: terrainData(terrainData)
		, airMoveArray(nullptr)
		, isUpdated(true)
#ifdef DEBUG_VIS
		, isVis(false)
		, toggleFrame(-1)
		, circuit(nullptr)
		, dbgDef(nullptr)
		, dbgPos(ZeroVector)
		, dbgType(1)
#endif
{
	squareSize   = terrainData->convertStoP;
	pathMapXSize = terrainData->sectorXSize;
	pathMapYSize = terrainData->sectorZSize;
	moveMapXSize = pathMapXSize + 2;  // +2 for passable edges
	moveMapYSize = pathMapYSize + 2;  // +2 for passable edges
	micropather  = new CMicroPather(this, pathMapXSize, pathMapYSize);

	areaData = terrainData->pAreaData.load();
	const std::vector<STerrainMapMobileType>& moveTypes = areaData->mobileType;
	moveArrays.reserve(moveTypes.size());

	const int totalcells = moveMapXSize * moveMapYSize;
	for (const STerrainMapMobileType& mt : moveTypes) {
		bool* moveArray = new bool[totalcells];
		moveArrays.push_back(moveArray);

		int k = 0;
		for (int z = 1; z < moveMapYSize - 1; ++z) {
			for (int x = 1; x < moveMapXSize - 1; ++x) {
				// NOTE: Not all passable sectors have area
				moveArray[z * moveMapXSize + x] = (mt.sector[k].area != nullptr);
				++k;
			}
		}

		// make sure that the edges are no-go
		for (int i = 0; i < moveMapXSize; ++i) {
			moveArray[i] = false;
			int k = moveMapXSize * (moveMapYSize - 1) + i;
			moveArray[k] = false;
		}
		for (int i = 0; i < moveMapYSize; ++i) {
			int k = i * moveMapXSize;
			moveArray[k] = false;
			k = i * moveMapXSize + moveMapXSize - 1;
			moveArray[k] = false;
		}
	}

	airMoveArray = new bool[totalcells];
	for (int i = 0; i < totalcells; ++i) {
		airMoveArray[i] = true;
	}
	// make sure that the edges are no-go
	for (int i = 0; i < moveMapXSize; ++i) {
		airMoveArray[i] = false;
		int k = moveMapXSize * (moveMapYSize - 1) + i;
		airMoveArray[k] = false;
	}
	for (int i = 0; i < moveMapYSize; ++i) {
		int k = i * moveMapXSize;
		airMoveArray[k] = false;
		k = i * moveMapXSize + moveMapXSize - 1;
		airMoveArray[k] = false;
	}

	blockArray.resize(terrainData->sectorXSize * terrainData->sectorZSize, 0);

	costMap.resize(terrainData->sectorXSize * terrainData->sectorZSize, -1.f);
}

CPathFinder::~CPathFinder()
{
	for (bool* ma : moveArrays) {
		delete[] ma;
	}
	delete[] airMoveArray;
	delete micropather;
}

void CPathFinder::UpdateAreaUsers(CTerrainManager* terrainManager)
{
	if (isUpdated) {
		return;
	}
	isUpdated = true;

	std::fill(blockArray.begin(), blockArray.end(), 0);
	const int granularity = squareSize / (SQUARE_SIZE * 2);
	const SBlockingMap& blockMap = terrainManager->GetBlockingMap();
	for (int z = 0; z < blockMap.rows; ++z) {
		for (int x = 0; x < blockMap.columns; ++x) {
			if (blockMap.IsStruct(x, z)) {
				const int moveX = x / granularity;
				const int moveY = z / granularity;
				++blockArray[moveY * terrainData->sectorXSize + moveX];
			}
		}
	}

	areaData = terrainData->GetNextAreaData();
	const std::vector<STerrainMapMobileType>& moveTypes = areaData->mobileType;
	const int blockThreshold = granularity * granularity / 4;  // 25% - blocked tile
	for (unsigned j = 0; j < moveTypes.size(); ++j) {
		const STerrainMapMobileType& mt = moveTypes[j];
		bool* moveArray = moveArrays[j];

		int k = 0;
		for (int z = 1; z < moveMapYSize - 1; ++z) {
			for (int x = 1; x < moveMapXSize - 1; ++x) {
				// NOTE: Not all passable sectors have area
				moveArray[z * moveMapXSize + x] = (mt.sector[k].area != nullptr) && (blockArray[k] < blockThreshold);
				++k;
			}
		}
	}
//	micropather->Reset();
}

void* CPathFinder::MoveXY2MoveNode(int x, int y) const
{
	return (void*) static_cast<intptr_t>(y * moveMapXSize + x);
}

void CPathFinder::MoveNode2MoveXY(void* node, int* x, int* y) const
{
	size_t index = (size_t)node;
	*y = index / moveMapXSize;
	*x = index - (*y * moveMapXSize);
}

AIFloat3 CPathFinder::MoveNode2Pos(void* node) const
{
	const size_t index = (size_t)node;

	float3 pos;
	size_t z = index / moveMapXSize;
	pos.z = (z - 1) * squareSize + squareSize / 2;
	pos.x = (index - (z * moveMapXSize) - 1) * squareSize + squareSize / 2;

	return pos;
}

void* CPathFinder::Pos2MoveNode(AIFloat3 pos) const
{
	return (void*) static_cast<intptr_t>(int(pos.z / squareSize + 1) * moveMapXSize + int((pos.x / squareSize + 1)));
}

void CPathFinder::Pos2MoveXY(AIFloat3 pos, int* x, int* y) const
{
	*x = int(pos.x / squareSize) + 1;
	*y = int(pos.z / squareSize) + 1;
}

void CPathFinder::Pos2PathXY(AIFloat3 pos, int* x, int* y) const
{
	*x = int(pos.x / squareSize);
	*y = int(pos.z / squareSize);
}

int CPathFinder::PathXY2PathIndex(int x, int y) const
{
	return y * pathMapXSize + x;
}

void CPathFinder::PathIndex2PathXY(int index, int* x, int* y) const
{
	*y = index / pathMapXSize;
	*x = index - (*y * pathMapXSize);
}

void CPathFinder::PathIndex2MoveXY(int index, int* x, int* y) const
{
	*y = index / pathMapXSize + 1;
	*x = index - (*y * pathMapXSize) + 1;
}

AIFloat3 CPathFinder::PathIndex2Pos(int index) const
{
	float3 pos;
	int z = index / pathMapXSize;
	pos.z = z * squareSize + squareSize / 2;
	pos.x = (index - (z * pathMapXSize)) * squareSize + squareSize / 2;

	return pos;
}

void CPathFinder::SetMapData(CCircuitUnit* unit, CThreatMap* threatMap, int frame)
{
	CCircuitDef* cdef = unit->GetCircuitDef();
	STerrainMapMobileType::Id mobileTypeId = cdef->GetMobileId();

	const std::vector<STerrainMapSector>& sectors = areaData->sector;
	bool* moveArray;
	float maxSlope;
	if (mobileTypeId < 0) {
		moveArray = airMoveArray;
		maxSlope = 1.f;
	} else {
		moveArray = moveArrays[mobileTypeId];
		maxSlope = std::max(areaData->mobileType[mobileTypeId].maxSlope, 1e-3f);
	}

	float* threatArray;
	CostFunc moveFun;
	CostFunc threatFun;
	// FIXME: DEBUG
	const float minElev = areaData->minElevation;
	float elevLen = std::max(areaData->maxElevation - areaData->minElevation, 1e-3f);
	if ((unit->GetPos(frame).y < .0f) && !cdef->IsSonarStealth()) {
		threatArray = threatMap->GetAmphThreatArray();  // cloak doesn't work under water
		moveFun = [&sectors, maxSlope, minElev, elevLen](int index) {
			return 2.f - 2.f * (sectors[index].maxElevation - minElev) / elevLen +
					(sectors[index].isWater ? 5.f : 0.f) + 4.f * sectors[index].maxSlope / maxSlope;
		};
		threatFun = [threatArray](int index) {
			return 10.f * threatArray[index];
		};
	} else if (unit->GetUnit()->IsCloaked()) {
		threatArray = threatMap->GetCloakThreatArray();
		moveFun = [&sectors, maxSlope](int index) {
			return sectors[index].maxSlope / maxSlope;
		};
		threatFun = [threatArray](int index) {
			return threatArray[index];
		};
	} else if (cdef->IsAbleToFly()) {
		threatArray = threatMap->GetAirThreatArray();
		moveFun = [](int index) {
			return 0.f;
		};
		threatFun = [threatArray](int index) {
			return 10.f * threatArray[index];
		};
	} else if (cdef->IsAmphibious()) {
		threatArray = threatMap->GetAmphThreatArray();
		if (maxSlope > SPIDER_SLOPE) {
			moveFun = [&sectors, minElev, elevLen](int index) {
				return 4.f - 4.f * (sectors[index].maxElevation - minElev) / elevLen +
						(sectors[index].isWater ? 5.f : 0.f) + 4.f * (1.f - sectors[index].maxSlope);
			};
			threatFun = [threatArray](int index) {
				return 10.f * threatArray[index];
			};
		} else {
			moveFun = [&sectors, maxSlope, minElev, elevLen](int index) {
				return 2.f - 2.f * (sectors[index].maxElevation - minElev) / elevLen +
						(sectors[index].isWater ? 5.f : 0.f) + 4.f * sectors[index].maxSlope / maxSlope;
			};
			threatFun = [threatArray](int index) {
				return 10.f * threatArray[index];
			};
		}
	} else {
		threatArray = threatMap->GetSurfThreatArray();
		moveFun = [&sectors, maxSlope, minElev, elevLen](int index) {
			return 2.f - 2.f * (sectors[index].maxElevation - minElev) / elevLen +
					(sectors[index].isWater ? 0.f : 4.f * sectors[index].maxSlope / maxSlope);
		};
		threatFun = [threatArray](int index) {
			return 10.f * threatArray[index];
		};
	}
	// FIXME: DEBUG

	micropather->SetMapData(moveArray, threatArray, moveFun, threatFun);
}

void CPathFinder::PreferPath(const IndexVec& path)
{
	assert(preferPath.empty());
	preferPath.reserve(path.size());
	for (int node : path) {
		preferPath.push_back(node);
		micropather->preferArray[node] = -COST_BASE / 4;
	}
}

void CPathFinder::UnpreferPath()
{
	for (int index : preferPath) {
		micropather->preferArray[index] = 0.f;
	}
	preferPath.clear();
}

/*
 * radius is in full res.
 * returns the path cost.
 */
float CPathFinder::MakePath(PathInfo& iPath, AIFloat3& startPos, AIFloat3& endPos, int radius, float maxThreat)
{
	iPath.Clear();

	CTerrainData::CorrectPosition(startPos);
	CTerrainData::CorrectPosition(endPos);

	float pathCost = 0.0f;
	radius /= squareSize;

	if (micropather->FindBestPathToPointOnRadius(Pos2MoveNode(startPos), Pos2MoveNode(endPos),
			radius, maxThreat, &iPath.path, &pathCost) == CMicroPather::SOLVED)
	{
		FillPathInfo(iPath);
	}

#ifdef DEBUG_VIS
	UpdateVis(iPath.path);
#endif

	return pathCost;
}

/*
 * WARNING: startPos must be correct
 */
float CPathFinder::PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius, float maxThreat)
{
	CTerrainData::CorrectPosition(endPos);

	float pathCost = 0.0f;
	radius /= squareSize;

	micropather->FindBestCostToPointOnRadius(Pos2MoveNode(startPos), Pos2MoveNode(endPos), radius, maxThreat, &pathCost);

	return pathCost;
}

float CPathFinder::FindBestPath(PathInfo& iPath, AIFloat3& startPos, float maxRange, F3Vec& possibleTargets, float maxThreat)
{
	float pathCost = 0.0f;

	// <maxRange> must always be >= squareSize, otherwise
	// <radius> will become 0 and the write to offsets[0]
	// below is undefined
	if (maxRange < float(squareSize)) {
		return pathCost;
	}

	iPath.Clear();

	const unsigned int radius = maxRange / squareSize;
	unsigned int offsetSize = 0;

	std::vector<std::pair<int, int> > offsets;
	std::vector<int> xend;

	// make a list with the points that will count as end nodes
	static std::vector<void*> endNodes;  // NOTE: micro-opt
//	endNodes.reserve(possibleTargets.size() * radius * 10);

	{
		const unsigned int DoubleRadius = radius * 2;
		const unsigned int SquareRadius = radius * radius;

		xend.resize(DoubleRadius + 1);
		offsets.resize(DoubleRadius * 5);

		for (size_t a = 0; a < DoubleRadius + 1; a++) {
			const float z = (int) (a - radius);
			const float floatsqrradius = SquareRadius;
			xend[a] = int(sqrt(floatsqrradius - z * z));
		}

		offsets[0].first = 0;
		offsets[0].second = 0;

		size_t index = 1;
		size_t index2 = 1;

		for (size_t a = 1; a < radius + 1; a++) {
			int endPosIdx = xend[a];
			int startPosIdx = xend[a - 1];

			while (startPosIdx <= endPosIdx) {
				assert(index < offsets.size());
				offsets[index].first = startPosIdx;
				offsets[index].second = a;
				startPosIdx++;
				index++;
			}

			startPosIdx--;
		}

		index2 = index;

		for (size_t a = 0; a < index2 - 2; a++) {
			assert(index < offsets.size());
			assert(a < offsets.size());
			offsets[index].first = offsets[a].first;
			offsets[index].second = DoubleRadius - (offsets[a].second);
			index++;
		}

		index2 = index;

		for (size_t a = 0; a < index2; a++) {
			assert(index < offsets.size());
			assert(a < offsets.size());
			offsets[index].first = -(offsets[a].first);
			offsets[index].second = offsets[a].second;
			index++;
		}

		for (size_t a = 0; a < index; a++) {
			assert(a < offsets.size());
//			offsets[a].first = offsets[a].first; // ??
			offsets[a].second = offsets[a].second - radius;
		}

		offsetSize = index;
	}

	static std::vector<void*> nodeTargets;  // NOTE: micro-opt
//	nodeTargets.reserve(possibleTargets.size());
	for (unsigned int i = 0; i < possibleTargets.size(); i++) {
		AIFloat3& f = possibleTargets[i];

		CTerrainData::CorrectPosition(f);
		void* node = Pos2MoveNode(f);
		NSMicroPather::PathNode* pn = micropather->GetNode(node);
		if (pn->isTarget) {
			continue;
		}
		pn->isTarget = 1;
		nodeTargets.push_back(node);

		int x, y;
		MoveNode2MoveXY(node, &x, &y);

		for (unsigned int j = 0; j < offsetSize; j++) {
			const int sx = x + offsets[j].first;
			const int sy = y + offsets[j].second;

			if (sx >= 0 && sx < moveMapXSize && sy >= 0 && sy < moveMapYSize) {
				endNodes.push_back(MoveXY2MoveNode(sx, sy));
			}
		}
	}
	for (void* node : nodeTargets) {
		micropather->GetNode(node)->isTarget = 0;
	}

	CTerrainData::CorrectPosition(startPos);

	if (micropather->FindBestPathToAnyGivenPoint(Pos2MoveNode(startPos), endNodes, nodeTargets,
			maxThreat, &iPath.path, &pathCost) == CMicroPather::SOLVED)
	{
		FillPathInfo(iPath);
	}

#ifdef DEBUG_VIS
	UpdateVis(iPath.path);
#endif

	endNodes.clear();
	nodeTargets.clear();
	return pathCost;
}

float CPathFinder::FindBestPathToRadius(PathInfo& posPath, AIFloat3& startPos, float radius, const AIFloat3& target, float maxThreat)
{
	F3Vec posTargets;
	posTargets.push_back(target);
	return FindBestPath(posPath, startPos, radius, posTargets, maxThreat);
}

/*
 * WARNING: startPos must be correct
 */
void CPathFinder::MakeCostMap(const AIFloat3& startPos)
{
	std::fill(costMap.begin(), costMap.end(), -1.f);
	micropather->MakeCostMap(Pos2MoveNode(startPos), costMap);
}

/*
 * WARNING: endPos must be correct
 */
float CPathFinder::GetCostAt(const AIFloat3& endPos, int radius) const
{
	float pathCost = -1.f;
	radius /= squareSize;

	int xm, ym;
	Pos2PathXY(endPos, &xm, &ym);

	const bool isInBox = (radius <= xm && xm <= pathMapXSize - 1 - radius)
			&& (radius <= ym && ym <= pathMapYSize - 1 - radius);
	auto minCost = isInBox ?
	std::function<float (float, int, int)>([this](float cost, int x, int y) -> float {
		const float costR = costMap[PathXY2PathIndex(x, y)];
		if (cost < 0.f) {
			return costR;
		}
		return (costR < 0.f) ? cost : std::min(cost, costR);
	}) :
	std::function<float (float, int, int)>([this](float cost, int x, int y) -> float {
		if ((x < 0) || (x > pathMapXSize - 1) || (y < 0) || (y > pathMapYSize - 1)) {
			return cost;
		}
		const float costR = costMap[PathXY2PathIndex(x, y)];
		if (cost < 0.f) {
			return costR;
		}
		return (costR < 0.f) ? cost : std::min(cost, costR);
	});

	// test 8 points
	pathCost = minCost(pathCost, xm - radius, ym);
	pathCost = minCost(pathCost, xm + radius, ym);
	pathCost = minCost(pathCost, xm, ym - radius);
	pathCost = minCost(pathCost, xm, ym + radius);

	const int r2 = radius * ISQRT_2 + 1;
	pathCost = minCost(pathCost, xm - r2, ym - r2);
	pathCost = minCost(pathCost, xm + r2, ym - r2);
	pathCost = minCost(pathCost, xm - r2, ym + r2);
	pathCost = minCost(pathCost, xm + r2, ym + r2);

	return pathCost;
}

size_t CPathFinder::RefinePath(IndexVec& path)
{
	if (micropather->threatArray[path[0]] > THREAT_EPSILON) {
		return 0;
	}

	int x0, y0;
	PathIndex2MoveXY(path[0], &x0, &y0);

	const CostFunc moveFun = micropather->moveFun;
	const float moveCost = moveFun(path[0]) + MOVE_EPSILON;

	// All octant line draw
	auto IsStraightLine = [this, x0, y0, moveCost, moveFun](int index) {
		// TODO: Remove node<->(x,y) conversions;
		//       Use Bresenham's 1-octant line algorithm
		int x1, y1;
		PathIndex2MoveXY(index, &x1, &y1);

		int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
		int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
		int err = dx + dy;  // error value e_xy
		for (int x = x0, y = y0;;) {
			int e2 = 2 * err;
			if (e2 >= dy) {  // e_xy + e_x > 0
				if (x == x1) break;
				err += dy; x += sx;
			}
			if (e2 <= dx) {  // e_xy + e_y < 0
				if (y == y1) break;
				err += dx; y += sy;
			}

			int idx = micropather->CanMoveNode2Index(MoveXY2MoveNode(x, y));
			if ((idx < 0) || (micropather->threatArray[idx] > THREAT_EPSILON) || (moveFun(idx) > moveCost)) {
				return false;
			}
		}
		return true;
	};

	int l = 1;
	int r = path.size() - 1;  // NOTE: start and end always present in path

	while (l <= r) {
		int m = (l + r) / 2;  // floor
		if (IsStraightLine(path[m])) {
			l = m + 1;  // ignore left half
		} else {
			r = m - 1;  // ignore right half
		}
	}

	return l - 1;
}

void CPathFinder::FillPathInfo(PathInfo& iPath)
{
	CMap* map = terrainData->GetMap();
	if (iPath.isLast) {
		float3 pos = PathIndex2Pos(iPath.path.back());
		pos.y = map->GetElevationAt(pos.x, pos.z);
		iPath.posPath.push_back(pos);
	} else {
		iPath.start = RefinePath(iPath.path);
		iPath.posPath.reserve(iPath.path.size() - iPath.start);

		// NOTE: only first few positions actually used due to frequent recalc.
		for (size_t i = iPath.start; i < iPath.path.size(); ++i) {
			float3 pos = PathIndex2Pos(iPath.path[i]);
			pos.y = map->GetElevationAt(pos.x, pos.z);
			iPath.posPath.push_back(pos);
		}
	}
}

#ifdef DEBUG_VIS
void CPathFinder::SetMapData(CThreatMap* threatMap)
{
	if ((dbgDef == nullptr) || (dbgType < 0) || (dbgType > 3)) {
		return;
	}

	STerrainMapMobileType::Id mobileTypeId = dbgDef->GetMobileId();
	const float maxSlope = (mobileTypeId < 0) ? 1.f : areaData->mobileType[mobileTypeId].maxSlope;
	const bool* moveArray = (mobileTypeId < 0) ? airMoveArray : moveArrays[mobileTypeId];
	const float* costArray[] = {
			threatMap->GetAirThreatArray(),
			threatMap->GetSurfThreatArray(),
			threatMap->GetAmphThreatArray(),
			threatMap->GetCloakThreatArray()
	};

	const float* threatArray = costArray[dbgType];
	const std::vector<STerrainMapSector>& sectors = areaData->sector;
	const CostFunc moveFun = [&sectors, maxSlope](int index) {
		return sectors[index].maxSlope / maxSlope;
	};
	const CostFunc threatFun = [threatArray](int index) {
		return threatArray[index];
	};

	micropather->SetMapData(moveArray, threatArray, moveFun, threatFun);
}

void CPathFinder::UpdateVis(const IndexVec& path)
{
	if (!isVis) {
		return;
	}

	CMap* map = terrainData->GetMap();
	Figure* fig = circuit->GetDrawer()->GetFigure();
	int figId = fig->DrawLine(ZeroVector, ZeroVector, 16.0f, true, FRAMES_PER_SEC * 5, 0);
	for (unsigned i = 1; i < path.size(); ++i) {
		AIFloat3 s = PathIndex2Pos(path[i - 1]);
		s.y = map->GetElevationAt(s.x, s.z);
		AIFloat3 e = PathIndex2Pos(path[i]);
		e.y = map->GetElevationAt(e.x, e.z);
		fig->DrawLine(s, e, 16.0f, true, FRAMES_PER_SEC * 20, figId);
	}
	fig->SetColor(figId, AIColor((float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX), 255);
	delete fig;
}

void CPathFinder::ToggleVis(CCircuitAI* circuit)
{
	if (toggleFrame >= circuit->GetLastFrame()) {
		return;
	}
	toggleFrame = circuit->GetLastFrame();

	isVis = !isVis;
	this->circuit = circuit;
}
#endif

} // namespace circuit
