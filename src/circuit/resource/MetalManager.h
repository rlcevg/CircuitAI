/*
 * MetalManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_METALMANAGER_H_
#define SRC_CIRCUIT_METALMANAGER_H_

#include "resource/MetalData.h"
#include "unit/CircuitUnit.h"
#include "lemon/adaptors.h"
#include "lemon/dijkstra.h"

namespace circuit {

class CCircuitAI;
class CMetalData;
class CScheduler;
template <class T> class CRagMatrix;

class CMetalManager {
public:
	CMetalManager(CCircuitAI* circuit, CMetalData* metalData);
	~CMetalManager();

private:
	void Init();

	void ParseMetalSpots();

public:
	bool HasMetalSpots() const { return (metalData->IsInitialized() && !metalData->IsEmpty()); }
	bool HasMetalClusters() const { return !metalData->GetClusters().empty(); }
	bool IsClusterizing() const { return metalData->IsClusterizing(); }

	void ClusterizeMetal(CCircuitDef* commDef);
	void SetAuthority(CCircuitAI* authority) { circuit = authority; }

	const CMetalData::Metals& GetSpots() const { return metalData->GetSpots(); }

	const int FindNearestSpot(const springai::AIFloat3& pos) const {
		return metalData->FindNearestSpot(pos);
	}
	const int FindNearestSpot(const springai::AIFloat3& pos, CMetalData::PointPredicate& predicate) const {
		return metalData->FindNearestSpot(pos, predicate);
	}
	void FindSpotsInRadius(const springai::AIFloat3& pos, const float radius, CMetalData::IndicesDists& outIndices) const {
		metalData->FindSpotsInRadius(pos, radius, outIndices);
	}
	void FindWithinRangeSpots(const springai::AIFloat3& posFrom, const springai::AIFloat3& posTo,
							  CMetalData::IndicesDists& outIndices) const;

	const int FindNearestCluster(const springai::AIFloat3& pos) const {
		return metalData->FindNearestCluster(pos);
	}
	const int FindNearestCluster(const springai::AIFloat3& pos, CMetalData::PointPredicate& predicate) const {
		return metalData->FindNearestCluster(pos, predicate);
	}

	const CMetalData::Clusters& GetClusters() const { return metalData->GetClusters(); }
	const CMetalData::ClusterGraph& GetClusterGraph() const { return metalData->GetClusterGraph(); }
	const CMetalData::ClusterCostMap& GetClusterEdgeCosts() const { return metalData->GetClusterEdgeCosts(); }

public:
	void SetOpenSpot(int index, bool value);
	void SetOpenSpot(const springai::AIFloat3& pos, bool value);
	bool IsOpenSpot(int index) const { return metalInfos[index].isOpen; }
	bool IsOpenSpot(const springai::AIFloat3& pos) const;
	void MarkAllyMexes();
	void MarkAllyMexes(const std::vector<CAllyUnit*>& mexes);
	int GetMexCount() const { return markedMexes.size(); }
	bool IsClusterFinished(int index) const {
		return clusterInfos[index].finishedCount >= GetClusters()[index].idxSpots.size();
	}
	bool IsClusterQueued(int index) const {
		return clusterInfos[index].queuedCount >= GetClusters()[index].idxSpots.size();
	}
	bool IsMexInFinished(int index) const;
	int GetCluster(int index) const { return metalInfos[index].clusterId; }

	int GetSpotToBuild(const springai::AIFloat3& pos, CMetalData::PointPredicate& predicate);
	int GetSpotToUpgrade(const springai::AIFloat3& pos, CMetalData::PointPredicate& predicate);

	bool IsSpotValid(int index, const springai::AIFloat3& pos) const;

	float GetSpotMinIncome() const { return metalData->GetSpotMinIncome(); }
	float GetSpotAvgIncome() const { return metalData->GetSpotAvgIncome(); }
	float GetSpotMaxIncome() const { return metalData->GetSpotMaxIncome(); }
	float GetSpotStdDeviation() const { return metalData->GetSpotStdDeviation(); }

	float GetClusterMinIncome() const { return metalData->GetClusterMinIncome(); }
	float GetClusterAvgIncome() const { return metalData->GetClusterAvgIncome(); }
	float GetClusterMaxIncome() const { return metalData->GetClusterMaxIncome(); }
	float GetClusterStdDeviation() const { return metalData->GetClusterStdDeviation(); }

private:
	class SafeCluster;
	class BuildCluster;
	class UpgradeCluster;
	using ClusterGraph = lemon::FilterNodes<const CMetalData::ClusterGraph, SafeCluster>;
	using ShortPath = lemon::Dijkstra<ClusterGraph, CMetalData::ClusterCostMap>;

	int GetSpotToDo(const springai::AIFloat3& pos, CMetalData::PointPredicate& predicate,
			std::function<CMetalData::ClusterGraph::Node (ShortPath* shortPath)>&& doGoal);

	CCircuitAI* circuit;
	CMetalData* metalData;

	struct SMetalInfo {
		bool isOpen;
		int clusterId;
	};
	struct SClusterInfo {
		unsigned int queuedCount;
		unsigned int finishedCount;
	};
	std::vector<SMetalInfo> metalInfos;
	std::vector<SClusterInfo> clusterInfos;

	int markFrame;
	struct SMex {
		ICoreUnit::Id unitId;
		int index;
	};
	std::deque<SMex> markedMexes;  // sorted by insertion

	SafeCluster* threatFilter;
	ClusterGraph* filteredGraph;
	ShortPath* shortPath;

	static std::vector<int> indices;  // NOTE: micro-opt
};

} // namespace circuit

#endif // SRC_CIRCUIT_METALMANAGER_H_
