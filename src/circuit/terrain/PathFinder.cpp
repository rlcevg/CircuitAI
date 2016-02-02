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
	pathMapXSize = terrainData->terrainWidth / squareSize;
	pathMapYSize = terrainData->terrainHeight / squareSize;
	micropather  = new CMicroPather(this, pathMapXSize, pathMapYSize);

	const std::vector<STerrainMapMobileType>& moveTypes = terrainData->pAreaData.load()->mobileType;
	moveArrays.reserve(moveTypes.size());

	int totalcells = pathMapXSize * pathMapYSize;
	for (const STerrainMapMobileType& mt : moveTypes) {
		bool* moveArray = new bool[totalcells];
		moveArrays.push_back(moveArray);

		for (int i = 0; i < totalcells; ++i) {
			// NOTE: Not all passable sectors have area
			moveArray[i] = (mt.sector[i].area != nullptr);
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

	blockArray.resize(totalcells, 0);
}

CPathFinder::~CPathFinder()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
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
			if (blockMap.IsStruct(x, z, SBlockingMap::StructMask::ALL)) {
				const int moveX = x / granularity;
				const int moveY = z / granularity;
				++blockArray[moveY * pathMapXSize + moveX];
			}
		}
	}

	const std::vector<STerrainMapMobileType>& moveTypes = terrainData->GetNextAreaData()->mobileType;
	const int totalcells = pathMapXSize * pathMapYSize;
	const int blockThreshold = granularity * granularity / 4;
	for (unsigned j = 0; j < moveTypes.size(); ++j) {
		const STerrainMapMobileType& mt = moveTypes[j];
		bool* moveArray = moveArrays[j];

		for (int i = 0; i < totalcells; ++i) {
			// NOTE: Not all passable sectors have area
			moveArray[i] = (mt.sector[i].area != nullptr) && (blockArray[i] < blockThreshold);
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

void* CPathFinder::XY2Node(int x, int y)
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
	pos.z = (index / pathMapXSize) * squareSize + squareSize / 2;
	pos.x = (index - ((index / pathMapXSize) * pathMapXSize)) * squareSize + squareSize / 2;

	return pos;
}

void* CPathFinder::Pos2Node(AIFloat3 pos)
{
	return (void*) static_cast<intptr_t>(int(pos.z / squareSize) * pathMapXSize + int((pos.x / squareSize)));
}

/*
 * radius is in full res.
 * returns the path cost.
 */
float CPathFinder::MakePath(F3Vec& posPath, AIFloat3& startPos, AIFloat3& endPos, int radius)
{
	path.clear();

	terrainData->CorrectPosition(startPos);
	terrainData->CorrectPosition(endPos);

	float pathCost = 0.0f;

	const int ex = int(endPos.x / squareSize);
	const int ey = int(endPos.z / squareSize);
	const int sy = int(startPos.z / squareSize);
	const int sx = int(startPos.x / squareSize);

	radius /= squareSize;

	if (micropather->FindBestPathToPointOnRadius(XY2Node(sx, sy), XY2Node(ex, ey), &path, &pathCost, radius) == CMicroPather::SOLVED) {
		posPath.reserve(path.size());

		// TODO: Consider performing transformations in place where move_along_path executed.
		//       Current task implementations recalc path every ~2 seconds,
		//       therefore only first few positions actually used.
		Map* map = terrainData->GetMap();
		for (void* node : path) {
			float3 mypos = Node2Pos(node);
			mypos.y = map->GetElevationAt(mypos.x, mypos.z);
			posPath.push_back(mypos);
		}
	}

#ifdef DEBUG_VIS
	UpdateVis(posPath);
#endif

	return pathCost;
}

/*
 * WARNING: startPos must be correct
 */
float CPathFinder::PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius)
{
	terrainData->CorrectPosition(endPos);

	float pathCost = 0.0f;

	const int ex = int(endPos.x / squareSize);
	const int ey = int(endPos.z / squareSize);
	const int sy = int(startPos.z / squareSize);
	const int sx = int(startPos.x / squareSize);

	radius /= squareSize;

	micropather->FindBestCostToPointOnRadius(XY2Node(sx, sy), XY2Node(ex, ey), &pathCost, radius);

	return pathCost;
}

/*
 * WARNING: startPos must be correct
 */
float CPathFinder::PathCostDirect(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius)
{
	terrainData->CorrectPosition(endPos);

	float pathCost = 0.0f;

	const int ex = int(endPos.x / squareSize);
	const int ey = int(endPos.z / squareSize);
	const int sy = int(startPos.z / squareSize);
	const int sx = int(startPos.x / squareSize);

	radius /= squareSize;

	micropather->FindDirectCostToPointOnRadius(XY2Node(sx, sy), XY2Node(ex, ey), &pathCost, radius);

	return pathCost;
}

float CPathFinder::FindBestPath(F3Vec& posPath, AIFloat3& startPos, float maxRange, F3Vec& possibleTargets)
{
	float pathCost = 0.0f;

	// <maxRange> must always be >= squareSize, otherwise
	// <radius> will become 0 and the write to offsets[0]
	// below is undefined
	if (maxRange < float(squareSize)) {
		return pathCost;
	}

	path.clear();

	const unsigned int radius = maxRange / squareSize;
	unsigned int offsetSize = 0;

	std::vector<std::pair<int, int> > offsets;
	std::vector<int> xend;

	// make a list with the points that will count as end nodes
	std::vector<void*> endNodes;
	endNodes.reserve(possibleTargets.size() * radius * 10);

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

	std::vector<void*> nodeTargets;
	nodeTargets.reserve(possibleTargets.size());
	for (unsigned int i = 0; i < possibleTargets.size(); i++) {
		AIFloat3& f = possibleTargets[i];

		terrainData->CorrectPosition(f);
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

	terrainData->CorrectPosition(startPos);

	if (micropather->FindBestPathToAnyGivenPoint(Pos2Node(startPos), endNodes, nodeTargets, &path, &pathCost) == CMicroPather::SOLVED) {
		posPath.reserve(path.size());

		Map* map = terrainData->GetMap();
		for (unsigned i = 0; i < path.size(); i++) {
			float3 mypos = Node2Pos(path[i]);
			mypos.y = map->GetElevationAt(mypos.x, mypos.z);
			posPath.push_back(mypos);
		}
	}

#ifdef DEBUG_VIS
	UpdateVis(posPath);
#endif

	return pathCost;
}

float CPathFinder::FindBestPathToRadius(F3Vec& posPath, AIFloat3& startPos, float radiusAroundTarget, const AIFloat3& target)
{
	F3Vec posTargets;
	posTargets.push_back(target);
	return FindBestPath(posPath, startPos, radiusAroundTarget, posTargets);
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

void CPathFinder::UpdateVis(const F3Vec& path)
{
	if (!isVis) {
		return;
	}

	Figure* fig = circuit->GetDrawer()->GetFigure();
	int figId = fig->DrawLine(ZeroVector, ZeroVector, 16.0f, true, FRAMES_PER_SEC * 5, 0);
	for (unsigned i = 1; i < path.size(); ++i) {
		fig->DrawLine(path[i - 1], path[i], 16.0f, true, FRAMES_PER_SEC * 20, figId);
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

//	auto node2pos = [this](void* node) {
//		const size_t index = (size_t)node;
//		float3 pos;
//		pos.z = (index / pathMapXSize) * squareSize;
//		pos.x = (index - ((index / pathMapXSize) * pathMapXSize)) * squareSize;
//		return pos;
//	};
//	Drawer* draw = circuit->GetDrawer();
//	if (isVis) {
//		for (int x = 0; x < pathMapXSize; ++x) {
//			for (int z = 1; z < pathMapYSize; ++z) {
//				AIFloat3 p0 = node2pos(XY2Node(x, z - 1));
//				AIFloat3 p1 = node2pos(XY2Node(x, z));
//				draw->AddLine(p0, p1);
//			}
//		}
//		for (int z = 0; z < pathMapYSize; ++z) {
//			for (int x = 1; x < pathMapXSize; ++x) {
//				AIFloat3 p0 = node2pos(XY2Node(x - 1, z));
//				AIFloat3 p1 = node2pos(XY2Node(x, z));
//				draw->AddLine(p0, p1);
//			}
//		}
//	} else {
//		for (int z = 0; z < pathMapYSize; ++z) {
//			for (int x = 0; x < pathMapXSize; ++x) {
//				AIFloat3 p = node2pos(XY2Node(x, z));
//				draw->DeletePointsAndLines(p);
//			}
//		}
//	}
}
#endif

} // namespace circuit
