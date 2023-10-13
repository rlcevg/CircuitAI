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
#include "terrain/path/QueryPathWide.h"
#include "terrain/path/QueryCostMap.h"
#include "terrain/path/QueryLineMap.h"
#include "terrain/TerrainData.h"
#include "terrain/TerrainManager.h"
#include "map/ThreatMap.h"
#include "scheduler/Scheduler.h"
#include "unit/CircuitUnit.h"
#include "util/Utils.h"
#include "util/Profiler.h"
#ifdef DEBUG_VIS
#include "CircuitAI.h"

#include "spring/SpringMap.h"

#include "Figure.h"
#endif

namespace circuit {

using namespace springai;
using namespace terrain;
using namespace NSMicroPather;

#define SPIDER_SLOPE		0.99f

std::vector<CPathFinder::SBlockCount> CPathFinder::blockArray;

CPathFinder::CPathFinder(CTerrainData* terrainData, int numThreads)
		: terrainData(terrainData)
		, pMoveData(&moveData0)
		, airMoveArray(nullptr)
		, isAreaUpdated(true)
		, queryId(0)
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

	micropathers.reserve(numThreads);
	for (int i = 0; i < numThreads; ++i) {
		NSMicroPather::CMicroPather* micropather = new CMicroPather(*this,
				pathMapXSize, pathMapYSize);
		micropathers.push_back(micropather);
	}

	areaData = terrainData->pAreaData.load();
	const std::vector<SMobileType>& moveTypes = areaData->mobileType;
	moveData0.moveArrays.reserve(moveTypes.size());
	moveData1.moveArrays.reserve(moveTypes.size());

	const int totalcells = moveMapXSize * moveMapYSize;
	for (const SMobileType& mt : moveTypes) {
		float* moveArray0 = new float[totalcells];
		float* moveArray1 = new float[totalcells];
		moveData0.moveArrays.push_back(moveArray0);
		moveData1.moveArrays.push_back(moveArray1);

		int k = 0;
		for (int z = 1; z < moveMapYSize - 1; ++z) {
			for (int x = 1; x < moveMapXSize - 1; ++x) {
				int index = z * moveMapXSize + x;
				// NOTE: Not all passable sectors have area
				moveArray1[index] = moveArray0[index] = (mt.sector[k].area != nullptr) ? COST_BASE : COST_BLOCK;
				++k;
			}
		}

		// make sure that the edges are no-go
		for (int i = 0; i < moveMapXSize; ++i) {
			moveArray1[i] = moveArray0[i] = COST_BLOCK;
			int k = moveMapXSize * (moveMapYSize - 1) + i;
			moveArray1[k] = moveArray0[k] = COST_BLOCK;
		}
		for (int i = 0; i < moveMapYSize; ++i) {
			int k = i * moveMapXSize;
			moveArray1[k] = moveArray0[k] = COST_BLOCK;
			k = i * moveMapXSize + moveMapXSize - 1;
			moveArray1[k] = moveArray0[k] = COST_BLOCK;
		}
	}

	airMoveArray = new float[totalcells];
	for (int i = 0; i < totalcells; ++i) {
		airMoveArray[i] = COST_BASE;
	}
	// make sure that the edges are no-go
	for (int i = 0; i < moveMapXSize; ++i) {
		airMoveArray[i] = COST_BLOCK;
		int k = moveMapXSize * (moveMapYSize - 1) + i;
		airMoveArray[k] = COST_BLOCK;
	}
	for (int i = 0; i < moveMapYSize; ++i) {
		int k = i * moveMapXSize;
		airMoveArray[k] = COST_BLOCK;
		k = i * moveMapXSize + moveMapXSize - 1;
		airMoveArray[k] = COST_BLOCK;
	}

	blockArray.resize(terrainData->sectorXSize * terrainData->sectorZSize, {0, 0});
}

CPathFinder::~CPathFinder()
{
	for (float* ma : moveData0.moveArrays) {
		delete[] ma;
	}
	for (float* ma : moveData1.moveArrays) {
		delete[] ma;
	}
	delete[] airMoveArray;
	for (NSMicroPather::CMicroPather* micropather : micropathers) {
		delete micropather;
	}
}

void CPathFinder::UpdateAreaUsers(CTerrainManager* terrainMgr)
{
	// NOTE: random teamId, according to std::unordered_set<CCircuitAI*> in CGameAttribute
	if (isAreaUpdated) {
		return;
	}
	isAreaUpdated = true;

	std::fill(blockArray.begin(), blockArray.end(), SBlockCount{0, 0});
	const int granularity = squareSize / (SQUARE_SIZE * 2);
	const SBlockingMap::SM notIgnore = STRUCT_BIT(MEX) | STRUCT_BIT(GEO);
	const SBlockingMap& blockMap = terrainMgr->GetBlockingMap();
	for (int z = 0; z < blockMap.rows; ++z) {
		for (int x = 0; x < blockMap.columns; ++x) {
			const int moveX = x / granularity;
			const int moveY = z / granularity;
			if (blockMap.IsStruct(x, z)) {
				++blockArray[moveY * terrainData->sectorXSize + moveX].structs;
			} else if (blockMap.IsBlocked(x, z, notIgnore)) {
				++blockArray[moveY * terrainData->sectorXSize + moveX].reserve;
			}
		}
	}

	std::vector<float*>& moveArrays = GetNextMoveData()->moveArrays;
	areaData = terrainData->GetNextAreaData();
	const std::vector<SMobileType>& moveTypes = areaData->mobileType;
	const float costStruct = COST_STRUCT / (granularity * granularity);
	const float costReserve = COST_RESERVE / (granularity * granularity);
	for (unsigned j = 0; j < moveTypes.size(); ++j) {
		const SMobileType& mt = moveTypes[j];
		float* moveArray = moveArrays[j];

		int k = 0;
		for (int z = 1; z < moveMapYSize - 1; ++z) {
			for (int x = 1; x < moveMapXSize - 1; ++x) {
				int index = z * moveMapXSize + x;
				// NOTE: Not all passable sectors have area
				moveArray[index] = (mt.sector[k].area == nullptr) ? COST_BLOCK
						: (blockArray[k].structs * costStruct + blockArray[k].reserve * costReserve + COST_BASE);
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

	AIFloat3 pos;
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

int CPathFinder::Pos2PathIndex(AIFloat3 pos) const
{
	return int(pos.z / squareSize) * pathMapXSize + int(pos.x / squareSize);
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
	AIFloat3 pos;
	int z = index / pathMapXSize;
	pos.z = z * squareSize + squareSize / 2;
	pos.x = (index - (z * pathMapXSize)) * squareSize + squareSize / 2;

	return pos;
}

/*
 * radius is in full res.
 */
std::shared_ptr<IPathQuery> CPathFinder::CreatePathSingleQuery(
		CCircuitUnit* unit, CThreatMap* threatMap,  // SetMapData
		const AIFloat3& startPos, const AIFloat3& endPos, float maxRange,
		NSMicroPather::HitFunc&& hitTest, float maxThreat, bool endPosOnly)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryPathSingle>(*this, MakeQueryId());
	CQueryPathSingle* query = static_cast<CQueryPathSingle*>(pQuery.get());

	FillMapData(query, unit, threatMap, startPos.y);
	query->InitQuery(startPos, endPos, maxRange, std::move(hitTest), maxThreat, endPosOnly);

	return pQuery;
}

std::shared_ptr<IPathQuery> CPathFinder::CreatePathMultiQuery(
		CCircuitUnit* unit, CThreatMap* threatMap,  // SetMapData
		const AIFloat3& startPos, float maxRange, const F3Vec& possibleTargets,
		NSMicroPather::HitFunc&& hitTest, bool withGoal, float maxThreat, bool endPosOnly)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryPathMulti>(*this, MakeQueryId());
	CQueryPathMulti* query = static_cast<CQueryPathMulti*>(pQuery.get());

	FillMapData(query, unit, threatMap, startPos.y);
	query->InitQuery(startPos, maxRange, possibleTargets, std::move(hitTest), withGoal, maxThreat, endPosOnly);

	return pQuery;
}

std::shared_ptr<IPathQuery> CPathFinder::CreatePathWideQuery(
		CCircuitUnit* unit, const CCircuitDef* cdef,  // SetMapData
		const AIFloat3& startPos, const AIFloat3& endPos, const IndexVec& targets)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryPathWide>(*this, MakeQueryId());
	CQueryPathWide* query = static_cast<CQueryPathWide*>(pQuery.get());

	FillMapData(query, unit, cdef, startPos.y);
	query->InitQuery(startPos, endPos, targets);

	return pQuery;
}

/*
 * WARNING: startPos must be correct
 */
std::shared_ptr<IPathQuery> CPathFinder::CreateCostMapQuery(
		CCircuitUnit* unit, CThreatMap* threatMap,  // SetMapData
		const AIFloat3& startPos, float maxThreat)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryCostMap>(*this, MakeQueryId());
	CQueryCostMap* query = static_cast<CQueryCostMap*>(pQuery.get());

	FillMapData(query, unit, threatMap, startPos.y);
	query->InitQuery(startPos, maxThreat);

	return pQuery;
}

std::shared_ptr<IPathQuery> CPathFinder::CreateLineMapQuery(
		CCircuitUnit* unit, CThreatMap* threatMap,  // SetMapData
		const AIFloat3& startPos)
{
	std::shared_ptr<IPathQuery> pQuery = std::make_shared<CQueryLineMap>(*this, MakeQueryId());
	CQueryLineMap* query = static_cast<CQueryLineMap*>(pQuery.get());

	FillMapData(query, unit, threatMap, startPos.y);
	query->InitQuery(threatMap->GetThreatMapWidth(), threatMap->GetSquareSize());

	return pQuery;
}

void CPathFinder::RunQuery(CScheduler* scheduler, const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
{
	switch (query->GetType()) {
		case IPathQuery::Type::SINGLE: {
			RunPathSingle(scheduler, query, std::move(onComplete));
		} break;
		case IPathQuery::Type::MULTI: {
			RunPathMulti(scheduler, query, std::move(onComplete));
		} break;
		case IPathQuery::Type::WIDE: {
			RunPathWide(scheduler, query, std::move(onComplete));
		} break;
		case IPathQuery::Type::COST: {
			RunCostMap(scheduler, query, std::move(onComplete));
		} break;
		default: break;
	}
}

CPathFinder::MoveType CPathFinder::GetMoveType(CCircuitUnit* unit, float elevation) const
{
	CCircuitDef* cdef = unit->GetCircuitDef();
	if ((elevation < .0f) && !cdef->IsSonarStealth()) {
		return MoveType::UNDERWATER;
	} else if (unit->IsInvisible()) {
		return MoveType::CLOAK;
	} else if (cdef->IsAbleToFly()) {
		return MoveType::AIR;
	} else {
		if (cdef->IsAbleToSwim()) {
			return MoveType::SWIM;
		} else if (cdef->IsAbleToDive()) {
			return MoveType::DIVE;
		} else {
			return MoveType::SURF;
		}
	}
}

CPathFinder::MoveType CPathFinder::GetMoveType(const CCircuitDef* cdef, float elevation) const
{
	if (elevation < .0f) {
		return MoveType::UNDERWATER;
	} else if (cdef->IsAbleToFly()) {
		return MoveType::AIR;
	} else {
		if (cdef->IsAbleToSwim()) {
			return MoveType::SWIM;
		} else if (cdef->IsAbleToDive()) {
			return MoveType::DIVE;
		} else {
			return MoveType::SURF;
		}
	}
}

CostFunc CPathFinder::GetMoveFun(MoveType mt, const CCircuitDef* cdef, float*& outMoveArray) const
{
	SMobileType::Id mobileTypeId = cdef->GetMobileId();

	const std::vector<SSector>& sectors = areaData->sector;
	float* moveArray;
	float maxSlope;
	if (mobileTypeId < 0) {
		moveArray = airMoveArray;
		maxSlope = 1.f;
	} else {
		moveArray = pMoveData.load()->moveArrays[mobileTypeId];
		maxSlope = std::max(areaData->mobileType[mobileTypeId].maxSlope, 1e-3f);
	}

	CostFunc moveFun;
	switch (mt) {
		case MoveType::UNDERWATER: {
			moveFun = [&sectors, maxSlope](int index) {
				return (sectors[index].isWater ? 2.f : 0.f) + 2.f * sectors[index].maxSlope / maxSlope;
			};
		} break;
		case MoveType::CLOAK: {
			moveFun = [&sectors, maxSlope](int index) {
				return sectors[index].maxSlope / maxSlope;
			};
		} break;
		case MoveType::AIR: {
			moveFun = [](int index) {
				return 0.f;
			};
		} break;
		case MoveType::SWIM: {
			if (maxSlope > SPIDER_SLOPE) {
				const float minElev = areaData->minElevation;
				float elevLen = std::max(areaData->maxElevation - areaData->minElevation, 1e-3f);
				moveFun = [&sectors, minElev, elevLen](int index) {
					return sectors[index].isWater ? 1.f : (2.f * (1.f - (sectors[index].maxElevation - minElev) / elevLen));
				};
			} else {
				moveFun = [&sectors, maxSlope](int index) {
					return sectors[index].isWater ? 1.f : (2.f * sectors[index].maxSlope / maxSlope);
				};
			}
		} break;
		case MoveType::DIVE: {
			if (maxSlope > SPIDER_SLOPE) {
				const float minElev = areaData->minElevation;
				float elevLen = std::max(areaData->maxElevation - areaData->minElevation, 1e-3f);
				moveFun = [&sectors, minElev, elevLen](int index) {
					return 2.f * (1.f - (sectors[index].maxElevation - minElev) / elevLen) +
							(sectors[index].isWater ? 2.f : 0.f);
				};
			} else {
				moveFun = [&sectors, maxSlope](int index) {
					return 2.f * sectors[index].maxSlope / maxSlope +
							(sectors[index].isWater ? 2.f : 0.f);
				};
			}
		} break;
		default:
		case MoveType::SURF: {
			if (maxSlope > SPIDER_SLOPE) {
				const float minElev = areaData->minElevation;
				float elevLen = std::max(areaData->maxElevation - areaData->minElevation, 1e-3f);
				moveFun = [&sectors, minElev, elevLen](int index) {
					return sectors[index].isWater ? 0.f : (2.f * (1.f - (sectors[index].maxElevation - minElev) / elevLen));
				};
			} else {
				moveFun = [&sectors, maxSlope](int index) {
					return sectors[index].isWater ? 0.f : (2.f * sectors[index].maxSlope / maxSlope);
				};
			}
		} break;
	}
	outMoveArray = moveArray;
	return moveFun;
}

CostFunc CPathFinder::GetThreatFun(MoveType mt, const CCircuitDef* cdef, CThreatMap* threatMap, float*& outThreatArray) const
{
	float* threatArray;
	CostFunc threatFun;
	switch (mt) {
		case MoveType::UNDERWATER: {
			threatArray = cdef->IsAbleToSwim()  // cloak doesn't work under water
					? threatMap->GetSwimThreatArray(cdef->GetMainRole())
					: threatMap->GetAmphThreatArray(cdef->GetMainRole());
			threatFun = [threatArray](int index) {
				return 2.f * threatArray[index];
			};
		} break;
		case MoveType::CLOAK: {
			threatArray = threatMap->GetCloakThreatArray();
			threatFun = [threatArray](int index) {
				return threatArray[index];
			};
		} break;
		case MoveType::AIR: {
			threatArray = threatMap->GetAirThreatArray(cdef->GetMainRole());
			threatFun = [threatArray](int index) {
				return 2.f * threatArray[index];
			};
		} break;
		case MoveType::SWIM: {
			threatArray = threatMap->GetSwimThreatArray(cdef->GetMainRole());
			threatFun = [threatArray](int index) {
				return 2.f * threatArray[index];
			};
		} break;
		case MoveType::DIVE: {
			threatArray = threatMap->GetAmphThreatArray(cdef->GetMainRole());
			threatFun = [threatArray](int index) {
				return 2.f * threatArray[index];
			};
		} break;
		default:
		case MoveType::SURF: {
			threatArray = threatMap->GetSurfThreatArray(cdef->GetMainRole());
			threatFun = [threatArray](int index) {
				return 2.f * threatArray[index];
			};
		} break;
	}
	outThreatArray = threatArray;
	return threatFun;
}

void CPathFinder::FillMapData(IPathQuery* query, CCircuitUnit* unit, const CCircuitDef* cdef, float elevation)
{
	float* moveArray;
	CostFunc moveFun = GetMoveFun(GetMoveType(cdef, elevation), cdef, moveArray);

	query->Init(moveArray, nullptr, std::move(moveFun), nullptr, unit);
}

void CPathFinder::FillMapData(IPathQuery* query, CCircuitUnit* unit, CThreatMap* threatMap, float elevation)
{
	MoveType mt = GetMoveType(unit, elevation);
	float* moveArray;
	float* threatArray;
	CostFunc moveFun = GetMoveFun(mt, unit->GetCircuitDef(), moveArray);
	CostFunc threatFun = GetThreatFun(mt, unit->GetCircuitDef(), threatMap, threatArray);

	query->Init(moveArray, threatArray, std::move(moveFun), std::move(threatFun), unit);
}

void CPathFinder::RunPathSingle(CScheduler* scheduler, const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
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
		ZoneScopedN(__PRETTY_FUNCTION__);

		this->MakePath(query, micropathers[threadNum]);
		return finish;
	}));
}

void CPathFinder::RunPathMulti(CScheduler* scheduler, const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
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
		ZoneScopedN(__PRETTY_FUNCTION__);

		this->FindBestPath(query, micropathers[threadNum]);
		return finish;
	}));
}

void CPathFinder::RunPathWide(CScheduler* scheduler, const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
{
	query->SetState(IPathQuery::State::PROCESS);
	std::shared_ptr<IMainJob> finish = CScheduler::PathedJob(query,
#ifdef DEBUG_VIS
	[this, onComplete](IPathQuery* query) {
		this->UpdateVis(static_cast<CQueryPathWide*>(query)->GetPathInfo()->path);
#else
	[onComplete](IPathQuery* query) {
#endif
		query->SetState(IPathQuery::State::READY);
		if (onComplete != nullptr) {
			onComplete(query);
		}
	});

	scheduler->RunParallelJob(CScheduler::PathJob(query, [this, finish](int threadNum, IPathQuery* query) {
		ZoneScopedN(__PRETTY_FUNCTION__);

		this->MakePathWide(query, micropathers[threadNum]);
		return finish;
	}));
}

void CPathFinder::RunCostMap(CScheduler* scheduler, const std::shared_ptr<IPathQuery>& query, PathCallback&& onComplete)
{
	query->SetState(IPathQuery::State::PROCESS);
	std::shared_ptr<IMainJob> finish = CScheduler::PathedJob(query, [onComplete](IPathQuery* query) {
		query->SetState(IPathQuery::State::READY);
		if (onComplete != nullptr) {
			onComplete(query);
		}
	});

	scheduler->RunParallelJob(CScheduler::PathJob(query, [this, finish](int threadNum, IPathQuery* query) {
		ZoneScopedN(__PRETTY_FUNCTION__);

		this->MakeCostMap(query, micropathers[threadNum]);
		return finish;
	}));
}

void CPathFinder::MakePath(IPathQuery* query, NSMicroPather::CMicroPather* micropather)
{
	CQueryPathSingle* q = static_cast<CQueryPathSingle*>(query);
	q->Prepare();

	const float* canMoveArray = q->GetCanMoveArray();
	const float* threatArray = q->GetThreatArray();
	const NSMicroPather::CostFunc& moveFun = q->GetMoveFun();
	const NSMicroPather::CostFunc& threatFun = q->GetThreatFun();
	const SAreaData* areaData = q->GetAreaData();

	AIFloat3& startPos = q->GetStartPosRef();
	AIFloat3& endPos = q->GetEndPosRef();
	const int radius = q->GetMaxRange() / squareSize;
	const NSMicroPather::HitFunc& hitTest = q->GetHitTest();
	const float maxThreat = q->GetMaxThreat();

	CPathInfo& iPath = q->GetPathInfoRef();
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

	const float* canMoveArray = q->GetCanMoveArray();
	const float* threatArray = q->GetThreatArray();
	const NSMicroPather::CostFunc& moveFun = q->GetMoveFun();
	const NSMicroPather::CostFunc& threatFun = q->GetThreatFun();
	const SAreaData* areaData = q->GetAreaData();

	AIFloat3& startPos = q->GetStartPosRef();
	F3Vec& possibleTargets = q->GetTargetsRef();
	const float maxRange = q->GetMaxRange();
	const NSMicroPather::HitFunc& hitTest = q->GetHitTest();
	const bool isWithGoal = q->IsWithGoal();
	const float maxThreat = q->GetMaxThreat();

	CPathInfo& iPath = q->GetPathInfoRef();
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

void CPathFinder::MakePathWide(IPathQuery* query, NSMicroPather::CMicroPather* micropather)
{
	CQueryPathWide* q = static_cast<CQueryPathWide*>(query);
	q->Prepare();

	const float* canMoveArray = q->GetCanMoveArray();
	const NSMicroPather::CostFunc& moveFun = q->GetMoveFun();
	const SAreaData* areaData = q->GetAreaData();

	AIFloat3& startPos = q->GetStartPosRef();
	AIFloat3& endPos = q->GetEndPosRef();
	IndexVec& targets = q->GetTargetsRef();
	const int howWide = squareSize / 32;  // squareSize ~= 32, 64, 128

	CPathInfo& iPath = q->GetPathInfoRef();
	float& pathCost = q->GetPathCostRef();

	iPath.Clear();

	CTerrainData::CorrectPosition(startPos);
	CTerrainData::CorrectPosition(endPos);

	std::vector<void*>& endNodes = micropather->endNodes;
	for (int i : targets) {
		int x, y;
		PathIndex2MoveXY(i, &x, &y);
		void* node = MoveXY2MoveNode(x, y);
		NSMicroPather::PathNode* pn = micropather->GetNode(node);
		if (pn->isEndNode == 0) {
			pn->isEndNode = 1;  // target node, avoid duplicates
			endNodes.push_back(node);
		}
	}
	void* endNode = Pos2MoveNode(endPos);
	for (int i = 0; i < 4; ++i) {
		void* node = (void*)((size_t)endNode + micropather->offsets[i]);
		NSMicroPather::PathNode* pn = micropather->GetNode(node);
		if (pn->isEndNode == 0) {
			pn->isEndNode = 1;  // target node, avoid duplicates
			endNodes.push_back(node);
		}
	}

	micropather->SetMapData(canMoveArray, nullptr, moveFun, nullptr, areaData);
	micropather->FindWidePathToBus(Pos2MoveNode(startPos), endNodes, howWide, &iPath.path, &pathCost);

	endNodes.clear();
}

void CPathFinder::MakeCostMap(IPathQuery* query, NSMicroPather::CMicroPather* micropather)
{
	CQueryCostMap* q = static_cast<CQueryCostMap*>(query);
	q->Prepare();

	const float* canMoveArray = q->GetCanMoveArray();
	const float* threatArray = q->GetThreatArray();
	NSMicroPather::CostFunc moveFun = q->GetMoveFun();
	NSMicroPather::CostFunc threatFun = q->GetThreatFun();
	const SAreaData* areaData = q->GetAreaData();

	const AIFloat3& startPos = q->GetStartPos();
	const float maxThreat = q->GetMaxThreat();
	std::vector<float>& costMap = q->GetCostMapRef();

	micropather->SetMapData(canMoveArray, threatArray, moveFun, threatFun, areaData);
	micropather->MakeCostMap(Pos2MoveNode(startPos), maxThreat, costMap);
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

	SMobileType::Id mobileTypeId = dbgDef->GetMobileId();
	const float maxSlope = (mobileTypeId < 0) ? 1.f : areaData->mobileType[mobileTypeId].maxSlope;
	const float* moveArray = (mobileTypeId < 0) ? airMoveArray : pMoveData.load()->moveArrays[mobileTypeId];
	const float* costArray[] = {
			threatMap->GetAirThreatArray(dbgDef->GetMainRole()),
			threatMap->GetSurfThreatArray(dbgDef->GetMainRole()),
			threatMap->GetAmphThreatArray(dbgDef->GetMainRole()),
			threatMap->GetCloakThreatArray()
	};

	const float* threatArray = costArray[dbgType];
	const std::vector<SSector>& sectors = areaData->sector;
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
