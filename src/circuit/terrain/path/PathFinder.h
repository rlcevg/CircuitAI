/*
 * PathFinder.h
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/PathFinder.h
 */

#ifndef SRC_CIRCUIT_TERRAIN_PATHFINDER_H_
#define SRC_CIRCUIT_TERRAIN_PATHFINDER_H_

#include "terrain/path/MicroPather.h"
#include "util/PathTask.h"
#include "util/Defines.h"

#include "System/Threading/SpringThreading.h"  // FIXME: Remove

namespace circuit {

class IPathQuery;
class CScheduler;
class CGameTask;
class CTerrainData;
class CTerrainManager;
class CCircuitUnit;
class CThreatMap;
struct SAreaData;
#ifdef DEBUG_VIS
class CCircuitAI;
class CCircuitDef;
#endif

class CPathFinder: public NSMicroPather::Graph {
public:
	struct SMoveData {
		std::vector<bool*> moveArrays;
	};

	CPathFinder(std::shared_ptr<CScheduler> scheduler, CTerrainData* terrainData);
	virtual ~CPathFinder();

	unsigned Checksum() const { return micropather->Checksum(); }

	void UpdateAreaUsers(CTerrainManager* terrainManager);
	void SetAreaUpdated(bool value) { isAreaUpdated = value; }

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

	std::shared_ptr<IPathQuery> CreatePathInfoQuery(CCircuitUnit* unit, CThreatMap* threatMap, int frame,
			const springai::AIFloat3& startPos, const springai::AIFloat3& endPos, int radius,
			float maxThreat = std::numeric_limits<float>::max());
	std::shared_ptr<IPathQuery> CreatePathMultiQuery(CCircuitUnit* unit, CThreatMap* threatMap, int frame,
			const springai::AIFloat3& startPos, float maxRange, const F3Vec& possibleTargets,
			float maxThreat = std::numeric_limits<float>::max());
	std::shared_ptr<IPathQuery> CreatePathCostQuery(CCircuitUnit* unit, CThreatMap* threatMap, int frame,
			const springai::AIFloat3& startPos, const springai::AIFloat3& endPos, int radius,
			float maxThreat = std::numeric_limits<float>::max());
	std::shared_ptr<IPathQuery> CreateCostMapQuery(CCircuitUnit* unit, CThreatMap* threatMap, int frame,
			const springai::AIFloat3& startPos);

	void RunQuery(std::shared_ptr<IPathQuery> query, PathFunc onComplete = nullptr);

	// FIXME: Remove
	void SetMapData(CCircuitUnit* unit, CThreatMap* threatMap, int frame);
	float MakePath(PathInfo& iPath, springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius,
			float maxThreat = std::numeric_limits<float>::max());
	float PathCost(const springai::AIFloat3& startPos, springai::AIFloat3& endPos, int radius,
			float maxThreat = std::numeric_limits<float>::max());
	float FindBestPath(PathInfo& iPath, springai::AIFloat3& startPos, float maxRange, F3Vec& possibleTargets,
			float maxThreat = std::numeric_limits<float>::max());
	// FIXME: Remove

	int GetSquareSize() const { return squareSize; }
	int GetPathMapXSize() const { return pathMapXSize; }
	int GetPathMapYSize() const { return pathMapYSize; }
	bool IsInPathMap(int x, int y, int radius) const {
		return (radius <= x && x <= pathMapXSize - 1 - radius)
			&& (radius <= y && y <= pathMapYSize - 1 - radius);
	}
	bool IsInPathMap(int x, int y) const {
		return (0 <= x) && (x <= pathMapXSize - 1)
			&& (0 <= y) && (y <= pathMapYSize - 1);
	}

	SMoveData* GetNextMoveData() {
		return (pMoveData.load() == &moveData0) ? &moveData1 : &moveData0;
	}

private:
	int MakeQueryId() { return queryId++; }
	void FillMapData(IPathQuery* query, CCircuitUnit* unit, CThreatMap* threatMap, int frame);

	void RunPathInfo(std::shared_ptr<IPathQuery> query, PathFunc onComplete = nullptr);
	void RunPathMulti(std::shared_ptr<IPathQuery> query, PathFunc onComplete = nullptr);
	void RunPathCost(std::shared_ptr<IPathQuery> query, PathFunc onComplete = nullptr);
	void RunCostMap(std::shared_ptr<IPathQuery> query, PathFunc onComplete = nullptr);

	void MakePath(IPathQuery* query);
	void FindBestPath(IPathQuery* query);
	void PathCost(IPathQuery* query);
	void MakeCostMap(IPathQuery* query);

	size_t RefinePath(IndexVec& path);
	void FillPathInfo(PathInfo& iPath);

	CTerrainData* terrainData;
	SAreaData* areaData;

	NSMicroPather::CMicroPather* micropather;
	SMoveData moveData0, moveData1;
	std::atomic<SMoveData*> pMoveData;
	bool* airMoveArray;
	static std::vector<int> blockArray;  // temporary array for moveArray construction
	bool isAreaUpdated;

	int squareSize;
	int moveMapXSize;  // +2 for edges
	int moveMapYSize;  // +2 for edges
	int pathMapXSize;
	int pathMapYSize;

	int queryId;
	std::shared_ptr<CScheduler> scheduler;
	NSMicroPather::CostFunc moveFun;
	spring::mutex microMutex;  // FIXME: Remove

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
