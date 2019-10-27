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
#include "terrain/ThreatMap.h"
#include "unit/CircuitUnit.h"
#include "util/utils.h"
#ifdef DEBUG_VIS
#include "CircuitAI.h"
#endif

#include "Map.h"
#ifdef DEBUG_VIS
#include "Figure.h"
#endif

namespace circuit {

using namespace springai;
using namespace NSMicroPather;

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
	pathMapXSize = terrainData->sectorXSize + 2;  // +2 for passable edges
	pathMapYSize = terrainData->sectorZSize + 2;  // +2 for passable edges
	micropather  = new CMicroPather(this, pathMapXSize, pathMapYSize);

	const std::vector<STerrainMapMobileType>& moveTypes = terrainData->pAreaData.load()->mobileType;
	moveArrays.reserve(moveTypes.size());

	const int totalcells = pathMapXSize * pathMapYSize;
	for (const STerrainMapMobileType& mt : moveTypes) {
		bool* moveArray = new bool[totalcells];
		moveArrays.push_back(moveArray);

//		for (int i = 0; i < totalcells; ++i) {
//			// NOTE: Not all passable sectors have area
//			moveArray[i] = (mt.sector[i].area != nullptr);
//		}
		int k = 0;
		for (int z = 1; z < pathMapYSize - 1; ++z) {
			for (int x = 1; x < pathMapXSize - 1; ++x) {
				// NOTE: Not all passable sectors have area
				moveArray[z * pathMapXSize + x] = (mt.sector[k].area != nullptr);
				++k;
			}
		}

		// make sure that the edges are no-go
		for (int i = 0; i < pathMapXSize; ++i) {
			moveArray[i] = false;
			int k = pathMapXSize * (pathMapYSize - 1) + i;
			moveArray[k] = false;
		}
		for (int i = 0; i < pathMapYSize; ++i) {
			int k = i * pathMapXSize;
			moveArray[k] = false;
			k = i * pathMapXSize + pathMapXSize - 1;
			moveArray[k] = false;
		}
	}

	airMoveArray = new bool[totalcells];
	for (int i = 0; i < totalcells; ++i) {
		airMoveArray[i] = true;
	}
	// make sure that the edges are no-go
	for (int i = 0; i < pathMapXSize; ++i) {
		airMoveArray[i] = false;
		int k = pathMapXSize * (pathMapYSize - 1) + i;
		airMoveArray[k] = false;
	}
	for (int i = 0; i < pathMapYSize; ++i) {
		int k = i * pathMapXSize;
		airMoveArray[k] = false;
		k = i * pathMapXSize + pathMapXSize - 1;
		airMoveArray[k] = false;
	}

	blockArray.resize(terrainData->sectorXSize * terrainData->sectorZSize, 0);

	costs.resize(totalcells, -1.f);
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
	for (int x = 0; x < blockMap.columns; ++x) {
		for (int z = 0; z < blockMap.rows; ++z) {
			if (blockMap.IsStruct(x, z, ~STRUCT_BIT(TERRA))) {
				const int moveX = x / granularity;
				const int moveY = z / granularity;
				++blockArray[moveY * terrainData->sectorXSize + moveX];
			}
		}
	}

	const std::vector<STerrainMapMobileType>& moveTypes = terrainData->GetNextAreaData()->mobileType;
	const int blockThreshold = granularity * granularity / 4;  // 25% - blocked tile
	for (unsigned j = 0; j < moveTypes.size(); ++j) {
		const STerrainMapMobileType& mt = moveTypes[j];
		bool* moveArray = moveArrays[j];

		int k = 0;
		for (int z = 1; z < pathMapYSize - 1; ++z) {
			for (int x = 1; x < pathMapXSize - 1; ++x) {
				// NOTE: Not all passable sectors have area
				moveArray[z * pathMapXSize + x] = (mt.sector[k].area != nullptr) && (blockArray[k] < blockThreshold);
				++k;
			}
		}

		// make sure that the edges are no-go
		for (int i = 0; i < pathMapXSize; ++i) {
			moveArray[i] = false;
			int k = pathMapXSize * (pathMapYSize - 1) + i;
			moveArray[k] = false;
		}
		for (int i = 0; i < pathMapYSize; ++i) {
			int k = i * pathMapXSize;
			moveArray[k] = false;
			k = i * pathMapXSize + pathMapXSize - 1;
			moveArray[k] = false;
		}
	}
	micropather->Reset();
}

void* CPathFinder::XY2Node(int x, int y) const
{
	return (void*) static_cast<intptr_t>(y * pathMapXSize + x);
}

void CPathFinder::Node2XY(void* node, int* x, int* y)
{
	size_t index = (size_t)node;
	*y = index / pathMapXSize;
	*x = index - (*y * pathMapXSize);
}

AIFloat3 CPathFinder::Node2Pos(void* node)
{
	const size_t index = (size_t)node;

	float3 pos;
	pos.z = (index / pathMapXSize - 1) * squareSize + squareSize / 2;
	pos.x = (index - ((index / pathMapXSize) * pathMapXSize) - 1) * squareSize + squareSize / 2;

	return pos;
}

void* CPathFinder::Pos2Node(AIFloat3 pos) const
{
	return (void*) static_cast<intptr_t>(int(pos.z / squareSize + 1) * pathMapXSize + int((pos.x / squareSize + 1)));
}

void CPathFinder::Pos2XY(AIFloat3 pos, int* x, int* y) const
{
	*x = int(pos.x / squareSize) + 1;
	*y = int(pos.z / squareSize) + 1;
}

void CPathFinder::SetMapData(CCircuitUnit* unit, CThreatMap* threatMap, int frame)
{
	CCircuitDef* cdef = unit->GetCircuitDef();
	STerrainMapMobileType::Id mobileTypeId = cdef->GetMobileId();
	bool* moveArray = (mobileTypeId < 0) ? airMoveArray : moveArrays[mobileTypeId];
	float* costArray;
	if ((unit->GetPos(frame).y < .0f) && !cdef->IsSonarStealth()) {
		costArray = threatMap->GetAmphThreatArray();  // cloak doesn't work under water
	} else if (unit->GetUnit()->IsCloaked()) {
		costArray = threatMap->GetCloakThreatArray();
	} else if (cdef->IsAbleToFly()) {
		costArray = threatMap->GetAirThreatArray();
	} else if (cdef->IsAmphibious()) {
		costArray = threatMap->GetAmphThreatArray();
	} else {
		costArray = threatMap->GetSurfThreatArray();
	}
	micropather->SetMapData(moveArray, costArray);
}

void CPathFinder::PreferPath(const VoidVec& path)
{
	assert(savedCost.empty());
	savedCost.reserve(path.size());
	for (void* node : path) {
		savedCost.push_back(std::make_pair(node, micropather->costArray[(size_t)node]));
		micropather->costArray[(size_t)node] -= THREAT_BASE / 4;
	}
}

void CPathFinder::UnpreferPath()
{
	for (const auto& pair : savedCost) {
		micropather->costArray[(size_t)pair.first] = pair.second;
	}
	savedCost.clear();
}

/*
 * radius is in full res.
 * returns the path cost.
 */
float CPathFinder::MakePath(PathInfo& iPath, AIFloat3& startPos, AIFloat3& endPos, int radius)
{
	iPath.Clear();

	CTerrainData::CorrectPosition(startPos);
	CTerrainData::CorrectPosition(endPos);

	float pathCost = 0.0f;
	radius /= squareSize;

	if (micropather->FindBestPathToPointOnRadius(Pos2Node(startPos), Pos2Node(endPos),
			&iPath.path, &pathCost, radius) == CMicroPather::SOLVED)
	{
		FillPathInfo(iPath);
	}

#ifdef DEBUG_VIS
	UpdateVis(iPath.path);
#endif

	return pathCost;
}

float CPathFinder::MakePath(PathInfo& iPath, AIFloat3& startPos, AIFloat3& endPos, int radius, float threat)
{
	iPath.Clear();

	CTerrainData::CorrectPosition(startPos);
	CTerrainData::CorrectPosition(endPos);

	float pathCost = 0.0f;
	radius /= squareSize;

	if (micropather->FindBestPathToPointOnRadius(Pos2Node(startPos), Pos2Node(endPos),
			&iPath.path, &pathCost, radius, threat) == CMicroPather::SOLVED)
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
float CPathFinder::PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius)
{
	CTerrainData::CorrectPosition(endPos);

	float pathCost = 0.0f;
	radius /= squareSize;

	micropather->FindBestCostToPointOnRadius(Pos2Node(startPos), Pos2Node(endPos), &pathCost, radius);

	return pathCost;
}

float CPathFinder::FindBestPath(PathInfo& iPath, AIFloat3& startPos, float maxRange, F3Vec& possibleTargets, bool safe)
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
		void* node = Pos2Node(f);
		NSMicroPather::PathNode* pn = micropather->GetNode((size_t)node);
		if (pn->isTarget) {
			continue;
		}
		pn->isTarget = 1;
		nodeTargets.push_back(node);

		int x, y;
		Node2XY(node, &x, &y);

		for (unsigned int j = 0; j < offsetSize; j++) {
			const int sx = x + offsets[j].first;
			const int sy = y + offsets[j].second;

			if (sx >= 0 && sx < pathMapXSize && sy >= 0 && sy < pathMapYSize) {
				endNodes.push_back(XY2Node(sx, sy));
			}
		}
	}
	for (void* node : nodeTargets) {
		micropather->GetNode((size_t)node)->isTarget = 0;
	}

	CTerrainData::CorrectPosition(startPos);

	int result = safe ? micropather->FindBestPathToAnyGivenPointSafe(Pos2Node(startPos), endNodes, nodeTargets, &iPath.path, &pathCost) :
						micropather->FindBestPathToAnyGivenPoint(Pos2Node(startPos), endNodes, nodeTargets, &iPath.path, &pathCost);
	if (result == CMicroPather::SOLVED) {
		FillPathInfo(iPath);
	}

#ifdef DEBUG_VIS
	UpdateVis(iPath.path);
#endif

	endNodes.clear();
	nodeTargets.clear();
	return pathCost;
}

float CPathFinder::FindBestPathToRadius(PathInfo& posPath, AIFloat3& startPos, float radiusAroundTarget, const AIFloat3& target)
{
	F3Vec posTargets;
	posTargets.push_back(target);
	return FindBestPath(posPath, startPos, radiusAroundTarget, posTargets);
}

/*
 * WARNING: startPos must be correct
 */
void CPathFinder::MakeCostMap(const AIFloat3& startPos)
{
	std::fill(costs.begin(), costs.end(), -1.f);
	micropather->MakeCostMap(Pos2Node(startPos), costs);
}

/*
 * WARNING: endPos must be correct
 */
float CPathFinder::GetCostAt(const AIFloat3& endPos, int radius) const
{
	float pathCost = -1.f;
	radius /= squareSize;

	int xm, ym;
	Pos2XY(endPos, &xm, &ym);

	const bool isInBox = (radius <= xm && xm <= pathMapXSize - 1 - radius)
			&& (radius <= ym && ym <= pathMapYSize - 1 - radius);
	auto minCost = isInBox ?
	std::function<float (float, int, int)>([this](float cost, int x, int y) -> float {
		const float costR = costs[(size_t)XY2Node(x, y)];
		if (cost < 0.f) {
			return costR;
		}
		return (costR < 0.f) ? cost : std::min(cost, costR);
	}) :
	std::function<float (float, int, int)>([this](float cost, int x, int y) -> float {
		if ((x < 1) || (x > pathMapXSize - 2)) {
			return cost;
		}
		if ((y < 1) || (y > pathMapYSize - 2)) {
			return cost;
		}
		const float costR = costs[(size_t)XY2Node(x, y)];
		if (cost < 0.f) {
			return costR;
		}
		return (costR < 0.f) ? cost : std::min(cost, costR);
	});

	// Circle draw
	int x = -radius, y = 0, err = 2 - 2 * radius;  // bottom left to top right
	do {
		pathCost = minCost(pathCost, xm - x, ym + y);  //   I. Quadrant +x +y
		pathCost = minCost(pathCost, xm - y, ym - x);  //  II. Quadrant -x +y
		pathCost = minCost(pathCost, xm + x, ym - y);  // III. Quadrant -x -y
		pathCost = minCost(pathCost, xm + y, ym + x);  //  IV. Quadrant +x -y
		radius = err;
		if (radius <= y) err += ++y * 2 + 1;  // e_xy + e_y < 0
		if (radius > x || err > y)  // e_xy + e_x > 0 or no 2nd y-step
			err += ++x * 2 + 1;  // -> x-step now
	} while (x < 0);

	return pathCost;
}

size_t CPathFinder::RefinePath(VoidVec& path)
{
	if (micropather->costArray[(size_t)path[0]] > THREAT_BASE + 1e-3f) {
		return 0;
	}

	int x0, y0;
	Node2XY(path[0], &x0, &y0);

	// All octant line draw
	auto IsStraightLine = [this, x0, y0](void* node) {
		// TODO: Remove node<->(x,y) conversions;
		//       Use Bresenham's 1-octant line algorithm
		int x1, y1;
		Node2XY(node, &x1, &y1);

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

			size_t index = (size_t)XY2Node(x, y);
			if (!micropather->canMoveArray[index]
				|| (micropather->costArray[index] > THREAT_BASE + 1e-3f))
			{
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
	Map* map = terrainData->GetMap();
	if (iPath.isLast) {
		float3 pos = Node2Pos(iPath.path.back());
		pos.y = map->GetElevationAt(pos.x, pos.z);
		iPath.posPath.push_back(pos);
	} else {
		iPath.start = RefinePath(iPath.path);
		iPath.posPath.reserve(iPath.path.size() - iPath.start);

		// NOTE: only first few positions actually used due to frequent recalc.
		for (size_t i = iPath.start; i < iPath.path.size(); ++i) {
			float3 pos = Node2Pos(iPath.path[i]);
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
	bool* moveArray = (mobileTypeId < 0) ? airMoveArray : moveArrays[mobileTypeId];
	float* costArray[] = {threatMap->GetAirThreatArray(), threatMap->GetSurfThreatArray(), threatMap->GetAmphThreatArray(), threatMap->GetCloakThreatArray()};
	micropather->SetMapData(moveArray, costArray[dbgType]);
}

void CPathFinder::UpdateVis(const VoidVec& path)
{
	if (!isVis) {
		return;
	}

	Map* map = terrainData->GetMap();
	Figure* fig = circuit->GetDrawer()->GetFigure();
	int figId = fig->DrawLine(ZeroVector, ZeroVector, 16.0f, true, FRAMES_PER_SEC * 5, 0);
	for (unsigned i = 1; i < path.size(); ++i) {
		AIFloat3 s = Node2Pos(path[i - 1]);
		s.y = map->GetElevationAt(s.x, s.z);
		AIFloat3 e = Node2Pos(path[i]);
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

//	Map* map = circuit->GetMap();
//	auto node2pos = [this, map](void* node) {
//		const size_t index = (size_t)node;
//		AIFloat3 pos;
//		pos.z = (index / pathMapXSize - 1) * squareSize;
//		pos.x = (index - ((index / pathMapXSize) * pathMapXSize) - 1) * squareSize;
//		pos.y = map->GetElevationAt(pos.x, pos.z) + SQUARE_SIZE;
//		return pos;
//	};
//	Drawer* draw = circuit->GetDrawer();
//	if (isVis) {
//		Figure* fig = circuit->GetDrawer()->GetFigure();
//		int figId = fig->DrawLine(ZeroVector, ZeroVector, 16.0f, false, FRAMES_PER_SEC * 5, 0);
//		for (int x = 1; x < pathMapXSize - 1; ++x) {
//			for (int z = 2; z < pathMapYSize - 1; ++z) {
//				AIFloat3 p0 = node2pos(XY2Node(x, z - 1));
//				AIFloat3 p1 = node2pos(XY2Node(x, z));
//				fig->DrawLine(p0, p1, 16.0f, false, FRAMES_PER_SEC * 200, figId);
//			}
//		}
//		for (int z = 1; z < pathMapYSize - 1; ++z) {
//			for (int x = 2; x < pathMapXSize - 1; ++x) {
//				AIFloat3 p0 = node2pos(XY2Node(x - 1, z));
//				AIFloat3 p1 = node2pos(XY2Node(x, z));
//				fig->DrawLine(p0, p1, 16.0f, false, FRAMES_PER_SEC * 200, figId);
//			}
//		}
//		fig->SetColor(figId, AIColor(1.0, 0., 0.), 255);
//		delete fig;
//	}
}
#endif

} // namespace circuit
