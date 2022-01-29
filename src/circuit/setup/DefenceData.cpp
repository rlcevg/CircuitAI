/*
 * DefenceData.cpp
 *
 *  Created on: Sep 20, 2015
 *      Author: rlcevg
 */

#include "setup/DefenceData.h"
#include "resource/MetalManager.h"
#include "scheduler/Scheduler.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/math/HierarchCluster.h"
#include "util/math/RagMatrix.h"
#include "util/math/EncloseCircle.h"
#include "util/Utils.h"
#include "json/json.h"

#include "spring/SpringMap.h"

namespace circuit {

using namespace springai;

CDefenceData::CDefenceData(CCircuitAI* circuit)
		: metalManager(nullptr)
{
	circuit->GetScheduler()->RunOnInit(CScheduler::GameJob(&CDefenceData::Init, this, circuit));

	ReadConfig(circuit);
}

CDefenceData::~CDefenceData()
{
}

void CDefenceData::ReadConfig(CCircuitAI* circuit)
{
	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const Json::Value& defence = root["defence"];
	const Json::Value& baseRad = defence["base_rad"];
	baseRadMin = baseRad.get((unsigned)0, 1000.f).asFloat();
	baseRadMax = baseRad.get((unsigned)1, 3000.f).asFloat();
	baseRange = utils::clamp(CTerrainManager::GetTerrainDiagonal() * 0.3f, baseRadMin, baseRadMax);
	const Json::Value& commRad = defence["comm_rad"];
	commRadBegin = commRad.get((unsigned)0, 1000.f).asFloat();
	const float commRadEnd = commRad.get((unsigned)1, 300.f).asFloat();
	commRadFraction = (commRadEnd - commRadBegin) / baseRange;

	const Json::Value& escort = defence["escort"];
	guardTaskNum = escort.get(unsigned(0), 2).asUInt();
	guardsNum = escort.get(unsigned(1), 1).asUInt();
	guardFrame = escort.get(unsigned(2), 600).asInt() * FRAMES_PER_SEC;

	pointRange = root["porcupine"].get("point_range", 600.f).asFloat();
}

void CDefenceData::Init(CCircuitAI* circuit)
{
	metalManager = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalManager->GetSpots();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	clusterInfos.resize(clusters.size());

	CMap* map = circuit->GetMap();
	const float maxDistance = pointRange;
	CHierarchCluster clust;
	CEncloseCircle enclose;

	for (unsigned k = 0; k < clusters.size(); ++k) {
		const CMetalData::MetalIndices& idxSpots = clusters[k].idxSpots;
		int nrows = idxSpots.size();
		CRagMatrix distmatrix(nrows);
		for (int i = 1; i < nrows; ++i) {
			for (int j = 0; j < i; ++j) {
				distmatrix(i, j) = spots[idxSpots[i]].position.distance2D(spots[idxSpots[j]].position);
			}
		}

		const CHierarchCluster::Clusters& iclusters = clust.Clusterize(distmatrix, maxDistance);

		DefPoints& defPoints = clusterInfos[k].defPoints;
		unsigned nclusters = iclusters.size();
		defPoints.reserve(nclusters);
		for (unsigned i = 0; i < nclusters; ++i) {
			std::vector<AIFloat3> points;
			points.reserve(iclusters[i].size());
			for (unsigned j = 0; j < iclusters[i].size(); ++j) {
				points.push_back(spots[idxSpots[iclusters[i][j]]].position);
			}
			enclose.MakeCircle(points);
			AIFloat3 pos = enclose.GetCenter();
			pos.y = map->GetElevationAt(pos.x, pos.z);
			defPoints.push_back({pos, .0f});
		}
	}
}

CDefenceData::SDefPoint* CDefenceData::GetDefPoint(const AIFloat3& pos, float defCost)
{
	int index = metalManager->FindNearestCluster(pos);
	if (index < 0) {
		return nullptr;
	}

	DefPoints& defPoints = clusterInfos[index].defPoints;
	unsigned idx = 0;
	float dist = pos.distance2D(defPoints[idx].position);
	for (unsigned i = 1; i < defPoints.size(); ++i) {
		if (defPoints[i].cost >= defCost) {
			float tmp = pos.distance2D(defPoints[i].position);
			if (tmp < dist) {
				tmp = dist;
				idx = i;
			}
		}
	}
	return &defPoints[idx];
}

void CDefenceData::SetBaseRange(float range)
{
	baseRange = utils::clamp(range, baseRadMin, baseRadMax);
}

} // namespace circuit
