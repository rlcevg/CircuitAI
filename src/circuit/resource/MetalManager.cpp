/*
 * ResourceManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "resource/MetalManager.h"
#include "module/EconomyManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "CircuitAI.h"
#include "util/math/RagMatrix.h"
#include "util/utils.h"

#include "Game.h"
#include "MoveData.h"
#include "Pathing.h"
#include "Map.h"

#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>

namespace circuit {

using namespace springai;

class CExitDSP: public std::exception {
public:
	CExitDSP() : std::exception() {}
	virtual const char* what() const throw() {
		return "DSP goal has been reached";
	}
};

class detect_cluster: public boost::dijkstra_visitor<> {
public:
	detect_cluster(CMetalManager* mgr, CMetalManager::MexPredicate& pred, std::vector<int>& outIdxs)
		: manager(mgr)
		, predicate(pred)
		, pindices(&outIdxs)
	{}
    template <class Vertex, class Graph>
	void examine_vertex(const Vertex u, const Graph& g) {
		if (manager->IsClusterQueued(u) || manager->IsClusterFinished(u)) {
			return;
		}
		for (int index : manager->GetClusters()[u].idxSpots) {
			if (predicate(index)) {
				pindices->push_back(index);
			}
		}
		if (!pindices->empty()) {
			throw CExitDSP();
		}
	}
	CMetalManager* manager;
	CMetalManager::MexPredicate predicate;
	std::vector<int>* pindices;
};

struct mex_tree {
	mex_tree() : threatMap(nullptr), pclusters(nullptr) {}
	mex_tree(CThreatMap* tm, const CMetalData::Clusters& cs)
		: threatMap(tm)
		, pclusters(&cs)
	{}
	bool operator()(const CMetalData::VertexDesc u) const {
		return threatMap->GetThreatAt((*pclusters)[u].position) <= THREAT_MIN;
	}
	CThreatMap* threatMap;
	const CMetalData::Clusters* pclusters;
};

CMetalManager::CMetalManager(CCircuitAI* circuit, CMetalData* metalData)
		: circuit(circuit)
		, metalData(metalData)
		, markFrame(-1)
{
	if (!metalData->IsInitialized()) {
		// TODO: Add metal zone and no-metal-spots maps support
		ParseMetalSpots();
	}
	metalInfos.resize(metalData->GetSpots().size(), {true, -1});
}

CMetalManager::~CMetalManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMetalManager::ParseMetalSpots()
{
	Game* game = circuit->GetGame();
	std::vector<CMetalData::SMetal> spots;

	int mexCount = game->GetRulesParamFloat("mex_count", -1);
	if (mexCount <= 0) {
		// FIXME: Replace metal-map workaround by own grid-spot generator
		Map* map = circuit->GetMap();
		Resource* metalRes = circuit->GetEconomyManager()->GetMetalRes();
		auto spotsPos = std::move(map->GetResourceMapSpotsPositions(metalRes));
		const unsigned width = map->GetWidth();
		const unsigned height = map->GetHeight();
		const float mapSize = (width / 64) * (height / 64);
		const float offset = (float)rand() / RAND_MAX * 50.f - 25.f;
		//  8x8  ~  80 spots +-25
		// 24x24 ~ 240 spots +-25
		unsigned mexCount = (240.f - 80.f) / (SQUARE(24.f) - SQUARE(8.f)) * (mapSize - SQUARE(8.f)) + 80.f + offset;
		unsigned inc;
		if (spotsPos.size() > mexCount) {
			inc = spotsPos.size() / mexCount;
			spots.reserve(mexCount);
		} else {
			inc = 1;
			spots.reserve(spotsPos.size());
		}
		CCircuitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();
		CTerrainManager* terrainManager = circuit->GetTerrainManager();
		const int xsize = mexDef->GetUnitDef()->GetXSize();
		const int zsize = mexDef->GetUnitDef()->GetZSize();
		for (unsigned i = 0; i < spotsPos.size(); i += inc) {
			const AIFloat3& pos = spotsPos[i];
			const unsigned x1 = int(pos.x) / SQUARE_SIZE - (xsize / 2), x2 = x1 + xsize;
			const unsigned z1 = int(pos.z) / SQUARE_SIZE - (zsize / 2), z2 = z1 + zsize;
			if ((x1 < x2) && (x2 < width) && (z1 < z2) && (z2 < height) &&
				terrainManager->CanBeBuiltAt(mexDef, pos))
			{
				const float y = map->GetElevationAt(pos.x, pos.z);
				spots.push_back({pos.y, AIFloat3(pos.x, y, pos.z)});
			}
		}

	} else {

		mexCount = std::min(mexCount, 1000);  // safety measure
		spots.resize(mexCount);
		for (int i = 0; i < mexCount; ++i) {
			std::string param;
			param = utils::int_to_string(i + 1, "mex_x%i");
			spots[i].position.x = game->GetRulesParamFloat(param.c_str(), 0.f);
			param = utils::int_to_string(i + 1, "mex_y%i");
			spots[i].position.y = game->GetRulesParamFloat(param.c_str(), 0.f);
			param = utils::int_to_string(i + 1, "mex_z%i");
			spots[i].position.z = game->GetRulesParamFloat(param.c_str(), 0.f);
			param = utils::int_to_string(i + 1, "mex_metal%i");
			spots[i].income = game->GetRulesParamFloat(param.c_str(), 0.f);
		}
	}

	metalData->Init(spots);
}

void CMetalManager::ClusterizeMetal(CCircuitDef* commDef)
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	metalData->SetClusterizing(true);

	const float maxDistance = circuit->GetEconomyManager()->GetPylonRange() * 2;
	const CMetalData::Metals& spots = metalData->GetSpots();
	int nrows = spots.size();

	std::shared_ptr<CRagMatrix> pdistmatrix = std::make_shared<CRagMatrix>(nrows);
	CRagMatrix& distmatrix = *pdistmatrix;
	if (nrows <= 300) {
		MoveData* moveData = commDef->GetUnitDef()->GetMoveData();
		int pathType = moveData->GetPathType();
		delete moveData;
		Pathing* pathing = circuit->GetPathing();
		for (int i = 1; i < nrows; i++) {
			for (int j = 0; j < i; j++) {
				float geomLength = spots[i].position.distance2D(spots[j].position);
				if (geomLength > 4 * maxDistance) {
					distmatrix(i, j) = geomLength;
				} else {
					float pathLength = pathing->GetApproximateLength(spots[i].position, spots[j].position, pathType, 0.0f);
					distmatrix(i, j) = (geomLength * 1.4f < pathLength) ? pathLength : geomLength;
				}
			}
		}
	} else {
		for (int i = 1; i < nrows; i++) {
			for (int j = 0; j < i; j++) {
				distmatrix(i, j) = spots[i].position.distance2D(spots[j].position);
			}
		}
	}

	// NOTE: Parallel clusterization was here,
	//       but bugs appeared: no communication with spring/lua
	metalData->Clusterize(maxDistance, pdistmatrix);
}

void CMetalManager::Init()
{
	clusterInfos.resize(GetClusters().size(), {0});
	for (unsigned i = 0; i < clusterInfos.size(); ++i) {
		for (int idx : GetClusters()[i].idxSpots) {
			metalInfos[idx].clusterId = i;
		}
	}
}

void CMetalManager::SetOpenSpot(int index, bool value)
{
	if (metalInfos[index].isOpen != value) {
		metalInfos[index].isOpen = value;
		clusterInfos[metalInfos[index].clusterId].queuedCount += value ? -1 : 1;
	}
}

void CMetalManager::SetOpenSpot(const springai::AIFloat3& pos, bool value)
{
	int index = FindNearestSpot(pos);
	if (index != -1) {
		SetOpenSpot(index, value);
	}
}

bool CMetalManager::IsOpenSpot(const springai::AIFloat3& pos) const
{
	int index = FindNearestSpot(pos);
	if (index != -1) {
		return IsOpenSpot(index);
	}
	return false;
}

void CMetalManager::MarkAllyMexes()
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}

	circuit->UpdateFriendlyUnits();
	CCircuitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();
	const CAllyTeam::Units& friendlies = circuit->GetFriendlyUnits();
	static std::vector<CAllyUnit*> tmpMexes;  // NOTE: micro-opt
//	tmpMexes.reserve(friendlies.size());
	for (auto& kv : friendlies) {
		CAllyUnit* unit = kv.second;
		if (*unit->GetCircuitDef() == *mexDef) {
			tmpMexes.push_back(unit);
		}
	}

	MarkAllyMexes(tmpMexes);

	tmpMexes.clear();
}

void CMetalManager::MarkAllyMexes(const std::vector<CAllyUnit*>& mexes)
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}
	markFrame = circuit->GetLastFrame();

	decltype(markedMexes) prevUnits = std::move(markedMexes);
	markedMexes.clear();
	auto first1  = mexes.begin();
	auto last1   = mexes.end();
	auto first2  = prevUnits.begin();
	auto last2   = prevUnits.end();
	auto d_first = std::back_inserter(markedMexes);
	auto addMex = [&d_first, this](CAllyUnit* unit) {
		SMex mex;
		mex.index = FindNearestSpot(unit->GetPos(this->circuit->GetLastFrame()));
		if (mex.index != -1) {
			mex.unitId = unit->GetId();
			*d_first++ = mex;
			clusterInfos[metalInfos[mex.index].clusterId].finishedCount++;
		}
	};
	auto delMex = [this](const SMex& mex) {
		clusterInfos[metalInfos[mex.index].clusterId].finishedCount--;
	};

	// @see std::set_symmetric_difference + std::set_intersection
	while (first1 != last1) {
		if (first2 == last2) {
			addMex(*first1);  // everything else in first1..last1 is new units
			while (++first1 != last1) {
				addMex(*first1);
			}
			break;
		}

		if ((*first1)->GetId() < first2->unitId) {
			addMex(*first1);  // new unit
			++first1;  // advance mexes
		} else {
			if (first2->unitId < (*first1)->GetId()) {
				delMex(*first2);  // dead unit
			} else {
				*d_first++ = *first2;  // old unit
				++first1;  // advance mexes
			}
            ++first2;  // advance prevUnits
		}
	}
	while (first2 != last2) {  // everything else in first2..last2 is dead units
		delMex(*first2++);
	}
}

bool CMetalManager::IsMexInFinished(int index) const
{
	// NOTE: finishedCount updated on lazy MarkAllyMexes call, thus can be invalid
	int idx = metalInfos[index].clusterId;
	return clusterInfos[idx].finishedCount >= GetClusters()[idx].idxSpots.size();
}

int CMetalManager::GetMexToBuild(const AIFloat3& pos, MexPredicate& predicate)
{
	int index = FindNearestCluster(pos);
	if (index < 0) {
		return -1;
	}
	MarkAllyMexes();

	mex_tree filter(circuit->GetThreatMap(), GetClusters());
	const CMetalData::Graph& graph = GetGraph();
	boost::filtered_graph<CMetalData::Graph, boost::keep_all, mex_tree> fg(graph, boost::keep_all(), filter);
	auto w_map = boost::get(&CMetalData::SEdge::weight, fg);
	static std::vector<int> indices;  // NOTE: micro-opt
	detect_cluster vis(this, predicate, indices);
	int result = -1;
	try {
		boost::dijkstra_shortest_paths(fg, boost::vertex(index, graph), boost::weight_map(w_map).visitor(vis));
	} catch (const CExitDSP& e) {
		float sqMinDist = std::numeric_limits<float>::max();
		for (int index : indices) {
			float sqDist = GetSpots()[index].position.SqDistance2D(pos);
			if (sqDist < sqMinDist) {
				sqMinDist = sqDist;
				result = index;
			}
		}
	}

	indices.clear();
	return result;
}

} // namespace circuit
