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
	void Pos2XY(springai::AIFloat3 pos, int* x, int* y);

	void SetMapData(CCircuitUnit* unit, CThreatMap* threatMap, int frame);
	void PreferPath(VoidVec& path);
	void UnpreferPath();

	unsigned Checksum() const { return micropather->Checksum(); }
	float MakePath(PathInfo& iPath, springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float MakePath(PathInfo& iPath, springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius, float threat);
	float PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float PathCostDirect(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float FindBestPath(PathInfo& iPath, springai::AIFloat3& startPos, float myMaxRange, F3Vec& possibleTargets, bool safe = true);
	float FindBestPathToRadius(PathInfo& iPath, springai::AIFloat3& startPos, float radiusAroundTarget, const springai::AIFloat3& target);

	int GetSquareSize() const { return squareSize; }

private:
	size_t RefinePath(VoidVec& path);
	void FillPathInfo(PathInfo& iPath);

	CTerrainData* terrainData;

	NSMicroPather::CMicroPather* micropather;
	bool* airMoveArray;
	std::vector<bool*> moveArrays;
	static std::vector<int> blockArray;
	bool isUpdated;

	int squareSize;
	int pathMapXSize;
	int pathMapYSize;

	std::vector<std::pair<void*, float>> savedCost;

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
	void UpdateVis(const VoidVec& path);
	void ToggleVis(CCircuitAI* circuit);
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATHFINDER_H_
