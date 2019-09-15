/*
 * ResourceManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCEMANAGER_H_
#define SRC_CIRCUIT_RESOURCEMANAGER_H_

#include "resource/MetalData.h"
#include "unit/CircuitUnit.h"

namespace circuit {

extern std::vector<CCircuitUnit*> tmpMexes;  // NOTE: micro-opt

class CCircuitAI;
class CMetalData;
class CScheduler;
class CGameTask;
class CRagMatrix;

class CMetalManager {
public:
	CMetalManager(CCircuitAI* circuit, CMetalData* metalData);
	virtual ~CMetalManager();

	void ParseMetalSpots();

	bool HasMetalSpots() const { return (metalData->IsInitialized() && !metalData->IsEmpty()); }
	bool HasMetalClusters() const { return !metalData->GetClusters().empty(); }
	bool IsClusterizing() const { return metalData->IsClusterizing(); }

	void ClusterizeMetal(CCircuitDef* commDef);
	void Init();
	void SetAuthority(CCircuitAI* authority) { circuit = authority; }

public:
	const CMetalData::Metals& GetSpots() const { return metalData->GetSpots(); }
	const int FindNearestSpot(const springai::AIFloat3& pos) const {
		return metalData->FindNearestSpot(pos);
	}
	const int FindNearestSpot(const springai::AIFloat3& pos, CMetalData::PointPredicate& predicate) const {
		return metalData->FindNearestSpot(pos, predicate);
	}

	const int FindNearestCluster(const springai::AIFloat3& pos) const {
		return metalData->FindNearestCluster(pos);
	}
	const int FindNearestCluster(const springai::AIFloat3& pos, CMetalData::PointPredicate& predicate) const {
		return metalData->FindNearestCluster(pos, predicate);
	}

	const CMetalData::Clusters& GetClusters() const { return metalData->GetClusters(); }
	const CMetalData::Graph& GetGraph() const { return metalData->GetGraph(); }

public:
	void SetOpenSpot(int index, bool value);
	void SetOpenSpot(const springai::AIFloat3& pos, bool value);
	bool IsOpenSpot(int index) const { return metalInfos[index].isOpen; }
	bool IsOpenSpot(const springai::AIFloat3& pos) const;
	void MarkAllyMexes();
	void MarkAllyMexes(const std::vector<CAllyUnit*>& mexes);
	bool IsClusterFinished(int index) const {
		return clusterInfos[index].finishedCount >= GetClusters()[index].idxSpots.size();
	}
	bool IsClusterQueued(int index) const {
		return clusterInfos[index].queuedCount >= GetClusters()[index].idxSpots.size();
	}
	bool IsMexInFinished(int index) const;
	int GetCluster(int index) const { return metalInfos[index].clusterId; }

	using MexPredicate = std::function<bool (int index)>;
	int GetMexToBuild(const springai::AIFloat3& pos, MexPredicate& predicate);

	float GetMinIncome() const { return metalData->GetMinIncome(); }
	float GetAvgIncome() const { return metalData->GetAvgIncome(); }
	float GetMaxIncome() const { return metalData->GetMaxIncome(); }

private:
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
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCEMANAGER_H_
