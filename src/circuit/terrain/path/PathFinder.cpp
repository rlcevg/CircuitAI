/*
 * PathFinder.cpp
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/PathFinder.cpp
 */

#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathSingle.h"
#include "terrain/path/QueryPathMulti.h"
#include "terrain/path/QueryCostMap.h"
#include "terrain/path/QueryLineMap.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainManager.h"
#include "map/ThreatMap.h"
#include "scheduler/Scheduler.h"
#include "unit/CircuitUnit.h"
#include "util/Utils.h"
#ifdef DEBUG_VIS
#include "CircuitAI.h"

#include "spring/SpringMap.h"

#include "Figure.h"
#endif

namespace circuit {

using namespace springai;
using namespace NSMicroPather;

#define SPIDER_SLOPE		0.99f

std::vector<int> CPathFinder::blockArray;

CPathFinder::CPathFinder(const std::shared_ptr<CScheduler>& scheduler, CTerrainData* terrainData)
		: terrainData(terrainData)
		, pMoveData(&moveData0)
		, airMoveArray(nullptr)
		, isAreaUpdated(true)
		, queryId(0)
		, scheduler(scheduler)
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
	pathMapXSize = terrainData->sectorXSize;
	pathMapYSize = terrainData->sectorZSize;
	moveMapXSize = pathMapXSize + 2;  // +2 for passable edges
	moveMapYSize = pathMapYSize + 2;  // +2 for passable edges

	int numThreads = scheduler->GetMaxWorkThreads();
	micropathers.reserve(numThreads);
	for (int i = 0; i < numThreads; ++i) {
		NSMicroPather::CMicroPather* micropather = new CMicroPather(*this,
				pathMapXSize, pathMapYSize);
		micropathers.push_back(micropather);
	}

	areaData = terrainData->pAreaData.load();
	const std::vector<STerrainMapMobileType>& moveTypes = areaData->mobileType;
	moveData0.moveArrays.reserve(moveTypes.size());
	moveData1.moveArrays.reserve(moveTypes.size());

	const int totalcells = moveMapXSize * moveMapYSize;
	for (const STerrainMapMobileType& mt : moveTypes) {
		bool* moveArray0 = new bool[totalcells];
		bool* moveArray1 = new bool[totalcells];
		moveData0.moveArrays.push_back(moveArray0);
		moveData1.moveArrays.push_back(moveArray1);

		int k = 0;
		for (int z = 1; z < moveMapYSize - 1; ++z) {
			for (int x = 1; x < moveMapXSize - 1; ++x) {
				int index = z * moveMapXSize + x;
				// NOTE: Not all passable sectors have area
				moveArray1[index] = moveArray0[index] = (mt.sector[k].area != nullptr);
				++k;
			}
		}

		// make sure that the edges are no-go
		for (int i = 0; i < moveMapXSize; ++i) {
			moveArray1[i] = moveArray0[i] = false;
			int k = moveMapXSize * (moveMapYSize - 1) + i;
			moveArray1[k] = moveArray0[k] = false;
		}
		for (int i = 0; i < moveMapYSize; ++i) {
			int k = i * moveMapXSize;
			moveArray1[k] = moveArray0[k] = false;
			k = i * moveMapXSize + moveMapXSize - 1;
			moveArray1[k] = moveArray0[k] = false;
		}
	}

	airMoveArray = new bool[totalcells];
	for (int i = 0; i < totalcells; ++i) {
		airMoveArray[i] = true;
	}
	// make sure that the edges are no-go
	for (int i = 0; i < moveMapXSize; ++i) {
		airMoveArray[i] = false;
		int k = moveMapXSize * (moveMapYSize - 1) + i;
		airMoveArray[k] = false;
	}
	for (int i = 0; i < moveMapYSize; ++i) {
		int k = i * moveMapXSize;
		airMoveArray[k] = false;
		k = i * moveMapXSize + moveMapXSize - 1;
		airMoveArray[k] = false;
	}

	blockArray.resize(terrainData->sectorXSize * terrainData->sectorZSize, 0);
}

CPathFinder::~CPathFinder()
{
	for (bool* ma : moveData0.moveArrays) {
		delete[] ma;
	}
	for (bool* ma : moveData1.moveArrays) {
		delete[] ma;
	}
	delete[] airMoveArray;
	for (NSMicroPather::CMicroPather* micropather : micropathers) {
		delete micropather;
	}
}

void CPathFinder::UpdateAreaUsers(CTerrainManager* terrainManager)
{
	if (isAreaUpdated) {
		return;
	}
	isAreaUpdated = true;

	std::fill(blockArray.begin(), blockArray.end(), 0);
	const int granularity = squareSize / (SQUARE_SIZE * 2);
	const SBlockingMap& blockMap = terrainManager->GetBlockingMap();
	for (int z = 0; z < blockMap.rows; ++z) {
		for (int x = 0; x < blockMap.columns; ++x) {
			if (blockMap.IsStruct(x, z)) {
				const int moveX = x / granularity;
				const int moveY = z / granularity;
				++blockArray[moveY * terrainData->sectorXSize + moveX];
			}
		}
	}

	std::vector<bool*>& moveArrays = GetNextMoveData()->moveArrays;
	areaData = terrainData->GetNextAreaData();
	const std::vector<STerrainMapMobileType>& moveTypes = areaData->mobileType;
	const int blockThreshold = granularity * granularity / 4;  // 25% - blocked tile
	for (unsigned j = 0; j < moveTypes.size(); ++j) {
		const STerrainMapMobileType& mt = moveTypes[j];
		bool* moveArray = moveArrays[j];

		int k = 0;
		for (int z = 1; z < moveMapYSize - 1; ++z) {
			for (int x = 1; x < moveMapXSize - 1; ++x) {
				int index = z * moveMapXSize + x;
				// NOTE: Not all passable sectors have area
				moveArray[index] = (mt.sector[k].area != nullptr) && (blockArray[k] < blockThreshold);
				++k;
			}
		}
	}
//	micropather->Reset();

	pMoveData = GetNextMoveData();
}

void* CPathFinder::MoveXY2MoveNode(int x, int y) const
{
	return (void*) static_cast<intptr_t>(y * moveMapXSize + x);
}

void CPathFinder::MoveNode2MoveXY(void* node, int* x, int* y) const
{
	size_t index = (size_t)node;
	*y = index / moveMapXSize;
	*x = index - (*y * moveMapXSize);
}

AIFloat3 CPathFinder::MoveNode2Pos(void* node) const
{
	const size_t index = (size_t)node;

	float3 pos;
	size_t z = index / moveMapXSize;
	pos.z = (z - 1) * squareSize + squareSize / 2;
	pos.x = (index - (z * moveMapXSize) - 1) * squareSize + squareSize / 2;

	return pos;
}

void* CPathFinder::Pos2MoveNode(AIFloat3 pos) const
{
	return (void*) static_cast<intptr_t>(int(pos.z / squareSize + 1) * moveMapXSize + int((pos.x / squareSize + 1)));
}

void CPathFinder::Pos2MoveXY(AIFloat3 pos, int* x, int* y) const
{
	*x = int(pos.x / squareSize) + 1;
	*y = int(pos.z / squareSize) + 1;
}

void CPathFinder::Pos2PathXY(AIFloat3 pos, int* x, int* y) const
{
	*x = int(pos.x / squareSize);
	*y = int(pos.z / squareSize);
}

int CPathFinder::PathXY2PathIndex(int x, int y) const
{
	return y * pathMapXSize + x;
}

void CPathFinder::PathIndex2PathXY(int index, int* x, int* y) const
{
	*y = index / pathMapXSize;
	*x = index - (*y * pathMapXSize);
}

void CPathFinder::PathIndex2MoveXY(int index, int* x, int* y) const
{
	int ty = index / pathMapXSize;
	*y = ty + 1;
	*x = index - (ty * pathMapXSize) + 1;
}

AIFloat3 CPathFinder::PathIndex2Pos(int index) const
{
	float3 pos;
	int z = index / pathMapXSize;
	pos.z = z * squareSize + squareSize / 2;
	pos.x = (index - (z * pathMapXSize)) * squareSize + squareSize / 2;

	return pos;
}

/*
 * radius is in full res.
 */
std::shared_ptr<IPathQuery> CPathFinder::CreatePathSingleQuery(
		CCircuitUnit* unit, CThreatMap* threatMap, int frame,  // SetMapData
		const AIFloat3& startPos, const AIFloat3& endPos, float maxRange,
		NSMicroPather::TestFunc&& hitTest, float maxThreat, bool endPosOnly)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryPathSingle>(*this, MakeQueryId());
	CQueryPathSingle* query = static_cast<CQueryPathSingle*>(pQuery.get());

	FillMapData(query, unit, threatMap, frame);
	query->InitQuery(startPos, endPos, maxRange, std::move(hitTest), maxThreat, endPosOnly);

	return pQuery;
}

std::shared_ptr<IPathQuery> CPathFinder::CreatePathMultiQuery(
		CCircuitUnit* unit, CThreatMap* threatMap, int frame,  // SetMapData
		const AIFloat3& startPos, float maxRange, const F3Vec& possibleTargets,
		NSMicroPather::TestFunc&& hitTest, bool withGoal, float maxThreat, bool endPosOnly)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryPathMulti>(*this, MakeQueryId());
	CQueryPathMulti* query = static_cast<CQueryPathMulti*>(pQuery.get());

	FillMapData(query, unit, threatMap, frame);
	query->InitQuery(startPos, maxRange, possibleTargets, std::move(hitTest), withGoal, maxThreat, endPosOnly);

	return pQuery;
}

/*
 * WARNING: startPos must be correct
 */
std::shared_ptr<IPathQuery> CPathFinder::CreateCostMapQuery(
		CCircuitUnit* unit, CThreatMap* threatMap, int frame,  // SetMapData
		const AIFloat3& startPos)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryCostMap>(*this, MakeQueryId());
	CQueryCostMap* query = static_cast<CQueryCostMap*>(pQuery.get());

	FillMapData(query, unit, threatMap, frame);
	query->InitQuery(startPos);

	return pQuery;
}

std::shared_ptr<IPathQuery> CPathFinder::CreateLineMapQuery(
		CCircuitUnit* unit, CThreatMap* threatMap, int frame)  // SetMapData
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryLineMap>(*this, MakeQueryId());
	CQueryLineMap* query = static_cast<CQueryLineMap*>(pQuery.get());

	FillMapData(query, unit, threatMap, frame);
	query->InitQuery(threatMap->GetThreatMapWidth(), threatMap->GetSquareSize());

	return pQuery;
}

void CPathFinder::RunQuery(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
{
	switch (query->GetType()) {
		case IPathQuery::Type::SINGLE: {
			RunPathSingle(query, std::move(onComplete));
		} break;
		case IPathQuery::Type::MULTI: {
			RunPathMulti(query, std::move(onComplete));
		} break;
		case IPathQuery::Type::COST: {
			RunCostMap(query, std::move(onComplete));
		} break;
		default: break;
	}
}

void CPathFinder::FillMapData(IPathQuery* query, CCircuitUnit* unit, CThreatMap* threatMap, int frame)
{
	CCircuitDef* cdef = unit->GetCircuitDef();
	STerrainMapMobileType::Id mobileTypeId = cdef->GetMobileId();

	const std::vector<STerrainMapSector>& sectors = areaData->sector;
	bool* moveArray;
	float maxSlope;
	if (mobileTypeId < 0) {
		moveArray = airMoveArray;
		maxSlope = 1.f;
	} else {
		moveArray = pMoveData.load()->moveArrays[mobileTypeId];
		maxSlope = std::max(areaData->mobileType[mobileTypeId].maxSlope, 1e-3f);
	}

	float* threatArray;
	CostFunc moveFun;
	CostFunc threatFun;
	// TODO: Re-organize and pre-calculate moveFun for each move-type
	if ((unit->GetPos(frame).y < .0f) && !cdef->IsSonarStealth()) {
		threatArray = threatMap->GetAmphThreatArray(cdef->GetMainRole());  // cloak doesn't work under water
		moveFun = [&sectors, maxSlope](int index) {
			return (sectors[index].isWater ? 2.f : 0.f) + 2.f * sectors[index].maxSlope / maxSlope;
		};
		threatFun = [threatArray](int index) {
			return 2.f * threatArray[index];
		};
	} else if (unit->GetUnit()->IsCloaked()) {
		threatArray = threatMap->GetCloakThreatArray();
		moveFun = [&sectors, maxSlope](int index) {
			return sectors[index].maxSlope / maxSlope;
		};
		threatFun = [threatArray](int index) {
			return threatArray[index];
		};
	} else if (cdef->IsAbleToFly()) {
		threatArray = threatMap->GetAirThreatArray(cdef->GetMainRole());
		moveFun = [](int index) {
			return 0.f;
		};
		threatFun = [threatArray](int index) {
			return 2.f * threatArray[index];
		};
	} else if (cdef->IsAmphibious()) {
		threatArray = threatMap->GetAmphThreatArray(cdef->GetMainRole());
		if (maxSlope > SPIDER_SLOPE) {
			const float minElev = areaData->minElevation;
			float elevLen = std::max(areaData->maxElevation - areaData->minElevation, 1e-3f);
			moveFun = [&sectors, minElev, elevLen](int index) {
				return 2.f * (1.f - (sectors[index].maxElevation - minElev) / elevLen) +
						(sectors[index].isWater ? 2.f : 0.f);
			};
			threatFun = [threatArray](int index) {
				return 2.f * threatArray[index];
			};
		} else {
			moveFun = [&sectors, maxSlope](int index) {
				return (sectors[index].isWater ? 2.f : 0.f) + 2.f * sectors[index].maxSlope / maxSlope;
			};
			threatFun = [threatArray](int index) {
				return 2.f * threatArray[index];
			};
		}
	} else {
		threatArray = threatMap->GetSurfThreatArray(cdef->GetMainRole());
		moveFun = [&sectors, maxSlope](int index) {
			return sectors[index].isWater ? 0.f : (2.f * sectors[index].maxSlope / maxSlope);
		};
		threatFun = [threatArray](int index) {
			return 2.f * threatArray[index];
		};
	}

	query->Init(moveArray, threatArray, std::move(moveFun), std::move(threatFun), unit);
}

void CPathFinder::RunPathSingle(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
{
	query->SetState(IPathQuery::State::PROCESS);
	std::shared_ptr<IMainJob> finish = CScheduler::PathedJob(query,
#ifdef DEBUG_VIS
	[this, onComplete](IPathQuery* query) {
		this->UpdateVis(static_cast<CQueryPathSingle*>(query)->GetPathInfo()->path);
#else
	[onComplete](IPathQuery* query) {
#endif
		query->SetState(IPathQuery::State::READY);
		if (onComplete != nullptr) {
			onComplete(query);
		}
	});
	scheduler->RunParallelJob(CScheduler::PathJob(query, [this, finish](int threadNum, IPathQuery* query) {
		this->MakePath(query, micropathers[threadNum]);
		return finish;
	}));
}

void CPathFinder::RunPathMulti(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
{
	query->SetState(IPathQuery::State::PROCESS);
	std::shared_ptr<IMainJob> finish = CScheduler::PathedJob(query,
#ifdef DEBUG_VIS
	[this, onComplete](IPathQuery* query) {
		this->UpdateVis(static_cast<CQueryPathMulti*>(query)->GetPathInfo()->path);
#else
	[onComplete](IPathQuery* query) {
#endif
		query->SetState(IPathQuery::State::READY);
		if (onComplete != nullptr) {
			onComplete(query);
		}
	});
	scheduler->RunParallelJob(CScheduler::PathJob(query, [this, finish](int threadNum, IPathQuery* query) {
		this->FindBestPath(query, micropathers[threadNum]);
		return finish;
	}));
}

void CPathFinder::RunCostMap(const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
{
	query->SetState(IPathQuery::State::PROCESS);
	std::shared_ptr<IMainJob> finish = CScheduler::PathedJob(query, [onComplete](IPathQuery* query) {
		query->SetState(IPathQuery::State::READY);
		if (onComplete != nullptr) {
			onComplete(query);
		}
	});
	scheduler->RunParallelJob(CScheduler::PathJob(query, [this, finish](int threadNum, IPathQuery* query) {
		this->MakeCostMap(query, micropathers[threadNum]);
		return finish;
	}));
}

void CPathFinder::MakePath(IPathQuery* query, NSMicroPather::CMicroPather* micropather)
{
	CQueryPathSingle* q = static_cast<CQueryPathSingle*>(query);
	q->Prepare();

	const bool* canMoveArray = q->GetCanMoveArray();
	const float* threatArray = q->GetThreatArray();
	const NSMicroPather::CostFunc& moveFun = q->GetMoveFun();
	const NSMicroPather::CostFunc& threatFun = q->GetThreatFun();
	const SAreaData* areaData = q->GetAreaData();

	AIFloat3& startPos = q->GetStartPosRef();
	AIFloat3& endPos = q->GetEndPosRef();
	const int radius = q->GetMaxRange() / squareSize;
	const NSMicroPather::TestFunc& hitTest = q->GetHitTest();
	const float maxThreat = q->GetMaxThreat();

	PathInfo& iPath = q->GetPathInfoRef();
	float& pathCost = q->GetPathCostRef();

	iPath.Clear();

	CTerrainData::CorrectPosition(startPos);
	CTerrainData::CorrectPosition(endPos);

	micropather->SetMapData(canMoveArray, threatArray, moveFun, threatFun, areaData);
	if (micropather->FindBestPathToPointOnRadius(Pos2MoveNode(startPos), Pos2MoveNode(endPos),
			radius, maxThreat, hitTest, &iPath.path, &pathCost) == CMicroPather::SOLVED)
	{
		micropather->FillPathInfo(iPath);
	}
}

void CPathFinder::FindBestPath(IPathQuery* query, NSMicroPather::CMicroPather* micropather)
{
	CQueryPathMulti* q = static_cast<CQueryPathMulti*>(query);
	q->Prepare();

	const bool* canMoveArray = q->GetCanMoveArray();
	const float* threatArray = q->GetThreatArray();
	const NSMicroPather::CostFunc& moveFun = q->GetMoveFun();
	const NSMicroPather::CostFunc& threatFun = q->GetThreatFun();
	const SAreaData* areaData = q->GetAreaData();

	AIFloat3& startPos = q->GetStartPosRef();
	F3Vec& possibleTargets = q->GetTargetsRef();
	const float maxRange = q->GetMaxRange();
	const NSMicroPather::TestFunc& hitTest = q->GetHitTest();
	const bool isWithGoal = q->IsWithGoal();
	const float maxThreat = q->GetMaxThreat();

	PathInfo& iPath = q->GetPathInfoRef();
	float& pathCost = q->GetPathCostRef();

	// <maxRange> must always be >= squareSize, otherwise
	// <radius> will become 0 and the write to offsets[0]
	// below is undefined
	if (maxRange < float(squareSize)) {
		return;
	}

	iPath.Clear();

	const unsigned int radius = maxRange / squareSize;
	unsigned int offsetSize = 0;

	std::vector<std::pair<int, int> > offsets;
	std::vector<int> xend;

	// make a list with the points that will count as end nodes
	std::vector<void*>& endNodes = micropather->endNodes;  // NOTE: micro-opt
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

	std::vector<void*>& nodeTargets = micropather->nodeTargets;  // NOTE: micro-opt
//	nodeTargets.reserve(possibleTargets.size());
	for (unsigned int i = 0; i < possibleTargets.size(); i++) {
		AIFloat3& f = possibleTargets[i];

		CTerrainData::CorrectPosition(f);
		void* node = Pos2MoveNode(f);
		NSMicroPather::PathNode* pn = micropather->GetNode(node);
		if (pn->isEndNode) {
			continue;
		}
		pn->isEndNode = 1;  // target node, avoid duplicates
		nodeTargets.push_back(node);

		int x, y;
		MoveNode2MoveXY(node, &x, &y);

		for (unsigned int j = 0; j < offsetSize; j++) {
			const int sx = x + offsets[j].first;
			const int sy = y + offsets[j].second;

			if (sx >= 1 && sx < moveMapXSize - 1 && sy >= 1 && sy < moveMapYSize - 1
				&& hitTest(int2(sx - 1, sy - 1), int2(x - 1, y - 1)))  // path-map, not move-map
			{
				endNodes.push_back(MoveXY2MoveNode(sx, sy));
			}
		}
		if (isWithGoal) {
			endNodes.push_back(MoveXY2MoveNode(x, y));  // in case hitTest rejected nodes on radius
		}
	}
	for (void* node : nodeTargets) {
		micropather->GetNode(node)->isEndNode = 0;
	}

	CTerrainData::CorrectPosition(startPos);

	micropather->SetMapData(canMoveArray, threatArray, moveFun, threatFun, areaData);
	if (micropather->FindBestPathToAnyGivenPoint(Pos2MoveNode(startPos), endNodes, nodeTargets,
			maxThreat, &iPath.path, &pathCost) == CMicroPather::SOLVED)
	{
		micropather->FillPathInfo(iPath);
	}

	endNodes.clear();
	nodeTargets.clear();
}

void CPathFinder::MakeCostMap(IPathQuery* query, NSMicroPather::CMicroPather* micropather)
{
	CQueryCostMap* q = static_cast<CQueryCostMap*>(query);
	q->Prepare();

	const bool* canMoveArray = q->GetCanMoveArray();
	const float* threatArray = q->GetThreatArray();
	NSMicroPather::CostFunc moveFun = q->GetMoveFun();
	NSMicroPather::CostFunc threatFun = q->GetThreatFun();
	const SAreaData* areaData = q->GetAreaData();

	const AIFloat3& startPos = q->GetStartPos();
	std::vector<float>& costMap = q->GetCostMapRef();

	micropather->SetMapData(canMoveArray, threatArray, moveFun, threatFun, areaData);
	micropather->MakeCostMap(Pos2MoveNode(startPos), costMap);
}

#ifdef DEBUG_VIS
std::shared_ptr<IPathQuery> CPathFinder::CreateDbgPathQuery(CThreatMap* threatMap,
		const AIFloat3& endPos, float maxRange, float maxThreat)
{
	if ((dbgDef == nullptr) || (dbgType < 0) || (dbgType > 3)) {
		return nullptr;
	}

	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryPathSingle>(*this, MakeQueryId());
	CQueryPathSingle* query = static_cast<CQueryPathSingle*>(pQuery.get());

	STerrainMapMobileType::Id mobileTypeId = dbgDef->GetMobileId();
	const float maxSlope = (mobileTypeId < 0) ? 1.f : areaData->mobileType[mobileTypeId].maxSlope;
	const bool* moveArray = (mobileTypeId < 0) ? airMoveArray : pMoveData.load()->moveArrays[mobileTypeId];
	const float* costArray[] = {
			threatMap->GetAirThreatArray(dbgDef->GetMainRole()),
			threatMap->GetSurfThreatArray(dbgDef->GetMainRole()),
			threatMap->GetAmphThreatArray(dbgDef->GetMainRole()),
			threatMap->GetCloakThreatArray()
	};

	const float* threatArray = costArray[dbgType];
	const std::vector<STerrainMapSector>& sectors = areaData->sector;
	CostFunc moveFun = [&sectors, maxSlope](int index) {
		return sectors[index].maxSlope / maxSlope;
	};
	CostFunc threatFun = [threatArray](int index) {
		return threatArray[index];
	};

	query->Init(moveArray, threatArray, std::move(moveFun), std::move(threatFun));
	query->InitQuery(dbgPos, endPos, maxRange, nullptr, maxThreat, false);

	return pQuery;
}

void CPathFinder::UpdateVis(const IndexVec& path)
{
	if (!isVis) {
		return;
	}

	CMap* map = terrainData->GetMap();
	Figure* fig = circuit->GetDrawer()->GetFigure();
	int figId = fig->DrawLine(ZeroVector, ZeroVector, 16.0f, true, FRAMES_PER_SEC * 5, 0);
	for (unsigned i = 1; i < path.size(); ++i) {
		AIFloat3 s = PathIndex2Pos(path[i - 1]);
		s.y = map->GetElevationAt(s.x, s.z);
		AIFloat3 e = PathIndex2Pos(path[i]);
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
}
#endif

} // namespace circuit
