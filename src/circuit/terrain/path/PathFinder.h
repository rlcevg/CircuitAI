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
#include "util/Defines.h"

#include <atomic>
#include <memory>
#include <functional>

namespace terrain {
	class CTerrainData;
	struct SAreaData;
}

namespace circuit {

class IPathQuery;
class CScheduler;
class CTerrainManager;
class CCircuitUnit;
class CThreatMap;
#ifdef DEBUG_VIS
class CCircuitAI;
class CCircuitDef;
#endif

class CPathFinder {
public:
	struct SMoveData {
		std::vector<float*> moveArrays;
	};
	using PathCallback = std::function<void (const IPathQuery* query)>;

	CPathFinder(const std::shared_ptr<CScheduler>& scheduler, terrain::CTerrainData* terrainData);
	virtual ~CPathFinder();

	void UpdateAreaUsers(CTerrainManager* terrainMgr);
	void SetAreaUpdated(bool value) { isAreaUpdated = value; }

	const terrain::SAreaData* GetAreaData() const { return areaData; }

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

	std::shared_ptr<IPathQuery> CreatePathSingleQuery(CCircuitUnit* unit, CThreatMap* threatMap,
			const springai::AIFloat3& startPos, const springai::AIFloat3& endPos, float maxRange,
			NSMicroPather::HitFunc&& hitTest = nullptr, float maxThreat = std::numeric_limits<float>::max(), bool endPosOnly = false);
	std::shared_ptr<IPathQuery> CreatePathMultiQuery(CCircuitUnit* unit, CThreatMap* threatMap,
			const springai::AIFloat3& startPos, float maxRange, const F3Vec& possibleTargets,
			NSMicroPather::HitFunc&& hitTest = nullptr, bool withGoal = false, float maxThreat = std::numeric_limits<float>::max(), bool endPosOnly = false);
	std::shared_ptr<IPathQuery> CreatePathWideQuery(CCircuitUnit* unit, const CCircuitDef* cdef,
			const springai::AIFloat3& startPos, const springai::AIFloat3& endPos, const IndexVec& targets);
	std::shared_ptr<IPathQuery> CreateCostMapQuery(CCircuitUnit* unit, CThreatMap* threatMap,
			const springai::AIFloat3& startPos, float maxThreat = std::numeric_limits<float>::max());
	std::shared_ptr<IPathQuery> CreateLineMapQuery(CCircuitUnit* unit, CThreatMap* threatMap,
			const springai::AIFloat3& startPos);

	void RunQuery(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete = nullptr);

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

private:
	SMoveData* GetNextMoveData() {
		return (pMoveData.load() == &moveData0) ? &moveData1 : &moveData0;
	}

	int MakeQueryId() { return queryId++; }
	enum class MoveType { UNDERWATER, CLOAK, AIR, SWIM, DIVE, SURF };
	MoveType GetMoveType(CCircuitUnit* unit, float elevation) const;
	MoveType GetMoveType(const CCircuitDef* cdef, float elevation) const;
	NSMicroPather::CostFunc GetMoveFun(MoveType mt, const CCircuitDef* cdef, float*& outMoveArray) const;
	NSMicroPather::CostFunc GetThreatFun(MoveType mt, const CCircuitDef* cdef, CThreatMap* threatMap, float*& outThreatArray) const;
	void FillMapData(IPathQuery* query, CCircuitUnit* unit, const CCircuitDef* cdef, float elevation);
	void FillMapData(IPathQuery* query, CCircuitUnit* unit, CThreatMap* threatMap, float elevation);

	void RunPathSingle(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete = nullptr);
	void RunPathMulti(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete = nullptr);
	void RunPathWide(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete = nullptr);
	void RunCostMap(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete = nullptr);

	void MakePath(IPathQuery* query, NSMicroPather::CMicroPather* micropather);
	void FindBestPath(IPathQuery* query, NSMicroPather::CMicroPather* micropather);
	void MakePathWide(IPathQuery* query, NSMicroPather::CMicroPather* micropather);
	void MakeCostMap(IPathQuery* query, NSMicroPather::CMicroPather* micropather);

	terrain::CTerrainData* terrainData;
	terrain::SAreaData* areaData;

	std::vector<NSMicroPather::CMicroPather*> micropathers;
	SMoveData moveData0, moveData1;
	std::atomic<SMoveData*> pMoveData;
	float* airMoveArray;
	struct SBlockCount {
		int structs;
		int reserve;
	};
	static std::vector<SBlockCount> blockArray;  // temporary array for moveArray construction
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
