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
#include "util/Defines.h"

namespace circuit {

class CTerrainData;
class CTerrainManager;
class CCircuitUnit;
class CThreatMap;
#ifdef DEBUG_VIS
class CCircuitAI;
class CCircuitDef;
#endif

class CPathFinder: public NSMicroPather::Graph {
public:
	CPathFinder(CTerrainData* terrainData);
	virtual ~CPathFinder();

	void UpdateAreaUsers(CTerrainManager* terrainManager);
	void SetUpdated(bool value) { isUpdated = value; }
	bool IsUpdated() const { return isUpdated; }

	void* XY2Node(int x, int y);
	void Node2XY(void* node, int* x, int* y);
	springai::AIFloat3 Node2Pos(void* node);
	void* Pos2Node(springai::AIFloat3 pos);

	void SetMapData(CCircuitUnit* unit, CThreatMap* threatMap, int frame);

	unsigned Checksum() const { return micropather->Checksum(); }
	float MakePath(F3Vec& posPath, springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float PathCostDirect(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float FindBestPath(F3Vec& posPath, springai::AIFloat3& startPos, float myMaxRange, F3Vec& possibleTargets);
	float FindBestPathToRadius(F3Vec& posPath, springai::AIFloat3& startPos, float radiusAroundTarget, const springai::AIFloat3& target);

	int GetSquareSize() const { return squareSize; }

private:
	CTerrainData* terrainData;

	NSMicroPather::CMicroPather* micropather;
	bool* airMoveArray;
	std::vector<bool*> moveArrays;
	std::vector<int> blockArray;
	bool isUpdated;

	int squareSize;
	int pathMapXSize;
	int pathMapYSize;

	std::vector<void*> path;

#ifdef DEBUG_VIS
private:
	bool isVis;
	int toggleFrame;
	CCircuitAI* circuit;
	CCircuitDef* dbgDef;
	springai::AIFloat3 dbgPos;
	int dbgType;
public:
	void SetDbgDef(CCircuitDef* cdef) { dbgDef = cdef; }
	CCircuitDef* GetDbgDef() const { return dbgDef; }
	void SetDbgPos(const springai::AIFloat3& pos) { dbgPos = pos; }
	const springai::AIFloat3& GetDbgPos() const { return dbgPos; }
	void SetDbgType(int type) { dbgType = type; }
	int GetDbgType() const { return dbgType; }
	void SetMapData(CThreatMap* threatMap);
	void UpdateVis(const F3Vec& path);
	void ToggleVis(CCircuitAI* circuit);
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATHFINDER_H_
