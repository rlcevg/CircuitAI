/*
 * DefenceMatrix.cpp
 *
 *  Created on: Sep 20, 2015
 *      Author: rlcevg
 */

#include "setup/DefenceMatrix.h"
#include "resource/MetalManager.h"
#include "CircuitAI.h"
#include "util/math/HierarchCluster.h"
#include "util/math/RagMatrix.h"
#include "util/math/EncloseCircle.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "Map.h"

namespace circuit {

using namespace springai;

CDefenceMatrix::CDefenceMatrix(CCircuitAI* circuit)
		: metalManager(nullptr)
{
	CScheduler* scheduler = circuit->GetScheduler().get();
	scheduler->RunParallelTask(CGameTask::emptyTask, std::make_shared<CGameTask>(&CDefenceMatrix::Init, this, circuit));
}

CDefenceMatrix::~CDefenceMatrix()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

CDefenceMatrix::SDefPoint* CDefenceMatrix::GetDefPoint(const AIFloat3& pos, float defCost)
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

void CDefenceMatrix::Init(CCircuitAI* circuit)
{
	metalManager = circuit->GetMetalManager();
	const CMetalData::Metals& spots = metalManager->GetSpots();
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	clusterInfos.resize(clusters.size());

	Map* map = circuit->GetMap();
	float maxDistance = circuit->GetCircuitDef("corllt")->GetMaxRange() * 0.75f * 2;
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

} // namespace circuit
