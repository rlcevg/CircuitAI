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

	void* MoveXY2MoveNode(int x, int y) const;
	void MoveNode2MoveXY(void* node, int* x, int* y) const;
	springai::AIFloat3 MoveNode2Pos(void* node) const;
	void* Pos2MoveNode(springai::AIFloat3 pos) const;
	void Pos2MoveXY(springai::AIFloat3 pos, int* x, int* y) const;
	void Pos2PathXY(springai::AIFloat3 pos, int* x, int* y) const;
	int PathXY2PathIndex(int x, int y) const;
	void PathIndex2PathXY(int index, int* x, int* y) const;
	void PathIndex2MoveXY(int index, int* x, int* y) const;
	springai::AIFloat3 PathIndex2Pos(int index) const;

	void SetMapData(CCircuitUnit* unit, CThreatMap* threatMap, int frame);
	void PreferPath(const IndexVec& path);
	void UnpreferPath();

	unsigned Checksum() const { return micropather->Checksum(); }
	float MakePath(PathInfo& iPath, springai::AIFloat3& startPos, springai::AIFloat3& endPos, float slope, int radius);
	float PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius);
	float FindBestPath(PathInfo& iPath, springai::AIFloat3& startPos, float myMaxRange, F3Vec& possibleTargets, bool safe = true);
	float FindBestPathToRadius(PathInfo& iPath, springai::AIFloat3& startPos, float radiusAroundTarget, const springai::AIFloat3& target);

	void MakeCostMap(const springai::AIFloat3& startPos);
	float GetCostAt(const springai::AIFloat3& endPos, int radius) const;

	int GetSquareSize() const { return squareSize; }

private:
	size_t RefinePath(IndexVec& path, NSMicroPather::CostFunc costFun);
	void FillPathInfo(PathInfo& iPath, NSMicroPather::CostFunc costFun);

	CTerrainData* terrainData;

	NSMicroPather::CMicroPather* micropather;
	float* slopeArray;
	bool* airMoveArray;
	std::vector<bool*> moveArrays;
	static std::vector<int> blockArray;
	bool isUpdated;

	int squareSize;
	int moveMapXSize;
	int moveMapYSize;
	int pathMapXSize;
	int pathMapYSize;

	std::vector<std::pair<int, float>> savedCost;  // <index, cost>
	std::vector<float> costMap;  // +2 with edges

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
	void UpdateVis(const IndexVec& path);
	void ToggleVis(CCircuitAI* circuit);
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATHFINDER_H_
