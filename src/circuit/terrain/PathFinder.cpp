/*
 * PathFinder.cpp
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/PathFinder.cpp
 */

#include "terrain/PathFinder.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "Map.h"
#ifdef DEBUG_VIS
#include "Figure.h"
#endif

namespace circuit {

using namespace springai;
using namespace NSMicroPather;

CPathFinder::CPathFinder(CCircuitAI* circuit)
		: circuit(circuit)
		, airMoveArray(nullptr)
#ifdef DEBUG_VIS
		, isVis(false)
		, toggleFrame(-1)
#endif
{
	squareSize   = circuit->GetTerrainManager()->GetConvertStoP();
	pathMapXSize = circuit->GetTerrainManager()->GetTerrainWidth() / squareSize;
	pathMapYSize = circuit->GetTerrainManager()->GetTerrainHeight() / squareSize;
	micropather  = new CMicroPather(this, pathMapXSize, pathMapYSize);
}

CPathFinder::~CPathFinder()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	// airMoveArray will be deleted inside loop
	for (bool* ma : moveArrays) {
		delete[] ma;
	}
	delete micropather;
}

void CPathFinder::Init()
{
	const std::vector<STerrainMapMobileType>& moveTypes = circuit->GetTerrainManager()->GetMobileTypes();
	moveArrays.reserve(moveTypes.size() + 1);

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
	moveArrays.push_back(airMoveArray);
}

void CPathFinder::UpdateAreaUsers()
{
	const std::vector<STerrainMapMobileType>& moveTypes = circuit->GetTerrainManager()->GetMobileTypes();
	int totalcells = pathMapXSize * pathMapYSize;
	for (int j = 0; j < moveTypes.size(); ++j) {
		const STerrainMapMobileType& mt = moveTypes[j];
		bool* moveArray = moveArrays[j];

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
	micropather->Reset();
}

void CPathFinder::SetMapData(STerrainMapMobileType::Id mobileTypeId)
{
	bool* moveArray = (mobileTypeId < 0) ? airMoveArray : moveArrays[mobileTypeId];
	micropather->SetMapData(moveArray, circuit->GetThreatMap()->GetThreatArray());
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
	pos.z = (index / pathMapXSize) * squareSize;
	pos.x = (index - ((index / pathMapXSize) * pathMapXSize)) * squareSize;

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

	circuit->GetTerrainManager()->CorrectPosition(startPos);
	circuit->GetTerrainManager()->CorrectPosition(endPos);

	float pathCost = 0.0f;

	const int ex = int(endPos.x / squareSize);
	const int ey = int(endPos.z / squareSize);
	const int sy = int(startPos.z / squareSize);
	const int sx = int(startPos.x / squareSize);

	radius /= squareSize;

	if (micropather->FindBestPathToPointOnRadius(XY2Node(sx, sy), XY2Node(ex, ey), &path, &pathCost, radius) == CMicroPather::SOLVED) {
		posPath.reserve(path.size());

		Map* map = circuit->GetMap();
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

float CPathFinder::PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius)
{
	path.clear();

	// startPos must be correct
	circuit->GetTerrainManager()->CorrectPosition(endPos);

	float pathCost = 0.0f;

	const int ex = int(endPos.x / squareSize);
	const int ey = int(endPos.z / squareSize);
	const int sy = int(startPos.z / squareSize);
	const int sx = int(startPos.x / squareSize);

	radius /= squareSize;

	micropather->FindBestPathToPointOnRadius(XY2Node(sx, sy), XY2Node(ex, ey), &path, &pathCost, radius);

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

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	for (unsigned int i = 0; i < possibleTargets.size(); i++) {
		AIFloat3& f = possibleTargets[i];
		int x, y;
		// TODO: make the circle here

		terrainManager->CorrectPosition(f);
		Node2XY(Pos2Node(f), &x, &y);

		for (unsigned int j = 0; j < offsetSize; j++) {
			const int sx = x + offsets[j].first;
			const int sy = y + offsets[j].second;

			if (sx >= 0 && sx < pathMapXSize && sy >= 0 && sy < pathMapYSize) {
				endNodes.push_back(XY2Node(sx, sy));
			}
		}
	}

	terrainManager->CorrectPosition(startPos);

	if (micropather->FindBestPathToAnyGivenPoint(Pos2Node(startPos), endNodes, &path, &pathCost) == CMicroPather::SOLVED) {
        posPath.reserve(path.size());

		Map* map = circuit->GetMap();
		for (unsigned i = 0; i < path.size(); i++) {
			int x, y;

			Node2XY(path[i], &x, &y);
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
void CPathFinder::UpdateVis(const F3Vec& path)
{
	if (!isVis) {
		return;
	}

	Figure* fig = circuit->GetDrawer()->GetFigure();
	int figId =	fig->DrawLine(ZeroVector, ZeroVector, 8.0f, true, FRAMES_PER_SEC * 20, 0);
	for (int i = 1; i < path.size(); ++i) {
		fig->DrawLine(path[i - 1], path[i], 8.0f, true, FRAMES_PER_SEC * 20, figId);
	}
	delete fig;
}

void CPathFinder::ToggleVis()
{
	if (toggleFrame >= circuit->GetLastFrame()) {
		return;
	}
	toggleFrame = circuit->GetLastFrame();

	isVis = !isVis;
}
#endif

} // namespace circuit