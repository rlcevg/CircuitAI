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

#include <atomic>

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

class CPathFinder {
public:
	struct SMoveData {
		std::vector<bool*> moveArrays;
	};

	CPathFinder(const std::shared_ptr<CScheduler>& scheduler, CTerrainData* terrainData);
	virtual ~CPathFinder();

	void UpdateAreaUsers(CTerrainManager* terrainManager);
	void SetAreaUpdated(bool value) { isAreaUpdated = value; }

	const FloatVec& GetHeightMap() const;

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

	std::shared_ptr<IPathQuery> CreatePathSingleQuery(CCircuitUnit* unit, CThreatMap* threatMap, int frame,
			const springai::AIFloat3& startPos, const springai::AIFloat3& endPos, float maxRange,
			float maxThreat = std::numeric_limits<float>::max(), bool endPosOnly = false);
	std::shared_ptr<IPathQuery> CreatePathMultiQuery(CCircuitUnit* unit, CThreatMap* threatMap, int frame,
			const springai::AIFloat3& startPos, float maxRange, const F3Vec& possibleTargets,
			float maxThreat = std::numeric_limits<float>::max(), bool endPosOnly = false);
	std::shared_ptr<IPathQuery> CreateCostMapQuery(CCircuitUnit* unit, CThreatMap* threatMap, int frame,
			const springai::AIFloat3& startPos);

	void RunQuery(const std::shared_ptr<IPathQuery>& query, PathedFunc&& onComplete = nullptr);

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

	void RunPathSingle(const std::shared_ptr<IPathQuery>& query, PathedFunc&& onComplete = nullptr);
	void RunPathMulti(const std::shared_ptr<IPathQuery>& query, PathedFunc&& onComplete = nullptr);
	void RunPathCost(const std::shared_ptr<IPathQuery>& query, PathedFunc&& onComplete = nullptr);
	void RunCostMap(const std::shared_ptr<IPathQuery>& query, PathedFunc&& onComplete = nullptr);

	void MakePath(IPathQuery* query, NSMicroPather::CMicroPather* micropather);
	void FindBestPath(IPathQuery* query, NSMicroPather::CMicroPather* micropather);
	void PathCost(IPathQuery* query, NSMicroPather::CMicroPather* micropather);
	void MakeCostMap(IPathQuery* query, NSMicroPather::CMicroPather* micropather);

	CTerrainData* terrainData;
	SAreaData* areaData;

	std::vector<NSMicroPather::CMicroPather*> micropathers;
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

#ifdef DEBUG_VIS
private:
	bool isVis;
	int toggleFrame;
	CCircuitAI* circuit;
	CCircuitDef* dbgDef;
	springai::AIFloat3 dbgPos;
	int dbgType;
	std::shared_ptr<IPathQuery> dbgQuery;
public:
	std::shared_ptr<IPathQuery> CreateDbgPathQuery(CThreatMap* threatMap,
			const springai::AIFloat3& endPos, float maxRange,
			float maxThreat = std::numeric_limits<float>::max());
	void SetDbgDef(CCircuitDef* cdef) { dbgDef = cdef; }
	CCircuitDef* GetDbgDef() const { return dbgDef; }
	void SetDbgPos(const springai::AIFloat3& pos) { dbgPos = pos; }
	const springai::AIFloat3& GetDbgPos() const { return dbgPos; }
	void SetDbgType(int type) { dbgType = type; }
	int GetDbgType() const { return dbgType; }
	void SetDbgQuery(const std::shared_ptr<IPathQuery>& query) { dbgQuery = query; }
	void UpdateVis(const IndexVec& path);
	void ToggleVis(CCircuitAI* circuit);
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_PATHFINDER_H_
