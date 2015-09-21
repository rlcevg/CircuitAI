/*
 * PathFinder.h
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/PathFinder.h
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATHFINDER_H_
#define SRC_CIRCUIT_TERRAIN_PATHFINDER_H_

#include "terrain/MicroPather.h"
#include "terrain/TerrainData.h"
#include "util/Defines.h"

namespace circuit {

class CCircuitAI;

class CPathFinder: public NSMicroPather::Graph {
public:
	CPathFinder(CCircuitAI* circuit);
	virtual ~CPathFinder();

	void Init();
	void UpdateAreaUsers();

	void* XY2Node(int x, int y);
	void Node2XY(void* node, int* x, int* y);
	springai::AIFloat3 Node2Pos(void* node);
	void* Pos2Node(springai::AIFloat3 pos);

	void SetMapData(STerrainMapMobileType::Id mobileTypeId);

	unsigned Checksum() const { return micropather->Checksum(); }
	float MakePath(F3Vec& posPath, springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float FindBestPath(F3Vec& posPath, springai::AIFloat3& startPos, float myMaxRange, F3Vec& possibleTargets);
	float FindBestPathToRadius(F3Vec& posPath, springai::AIFloat3& startPos, float radiusAroundTarget, const springai::AIFloat3& target);

private:
	CCircuitAI* circuit;

	NSMicroPather::CMicroPather* micropather;
	bool* airMoveArray;
	std::vector<bool*> moveArrays;

	int squareSize;
	int pathMapXSize;
	int pathMapYSize;

	std::vector<void*> path;

#ifdef DEBUG_VIS
private:
	bool isVis;
	int toggleFrame;
public:
	void UpdateVis(const F3Vec& path);
	void ToggleVis();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATHFINDER_H_
