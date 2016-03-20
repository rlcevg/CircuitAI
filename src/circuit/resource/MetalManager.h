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

class CCircuitAI;
class CMetalData;
class CScheduler;
class CGameTask;
class CRagMatrix;

class CMetalManager {
public:
	CMetalManager(CCircuitAI* circuit, CMetalData* metalData);
	virtual ~CMetalManager();

	void ParseMetalSpots(const char* metalJson);
	void ParseMetalSpots(springai::Game* game);

	bool HasMetalSpots() const { return (metalData->IsInitialized() && !metalData->IsEmpty()); }
	bool HasMetalClusters() const { return !metalData->GetClusters().empty(); }
	bool IsClusterizing() const { return metalData->IsClusterizing(); }

	void ClusterizeMetal();
	void Init();

public:
	const CMetalData::Metals& GetSpots() const { return metalData->GetSpots(); }
	const int FindNearestSpot(const springai::AIFloat3& pos) const {
		return metalData->FindNearestSpot(pos);
	}
	const int FindNearestSpot(const springai::AIFloat3& pos, CMetalData::MetalPredicate& predicate) const {
		return metalData->FindNearestSpot(pos, predicate);
	}
	const CMetalData::MetalIndices FindNearestSpots(const springai::AIFloat3& pos, int num) const {
		return metalData->FindNearestSpots(pos, num);
	}
	const CMetalData::MetalIndices FindNearestSpots(const springai::AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const {
		return metalData->FindNearestSpots(pos, num, predicate);
	}
	const CMetalData::MetalIndices FindWithinDistanceSpots(const springai::AIFloat3& pos, float maxDistance) const {
		return metalData->FindWithinDistanceSpots(pos, maxDistance);
	}
	const CMetalData::MetalIndices FindWithinRangeSpots(const springai::AIFloat3& posFrom, const springai::AIFloat3& posTo) const {
		return metalData->FindWithinRangeSpots(posFrom, posTo);
	}

	const int FindNearestCluster(const springai::AIFloat3& pos) const {
		return metalData->FindNearestCluster(pos);
	}
	const int FindNearestCluster(const springai::AIFloat3& pos, CMetalData::MetalPredicate& predicate) const {
		return metalData->FindNearestCluster(pos, predicate);
	}
	const CMetalData::MetalIndices FindNearestClusters(const springai::AIFloat3& pos, int num) const {
		return metalData->FindNearestClusters(pos, num);
	}
	const CMetalData::MetalIndices FindNearestClusters(const springai::AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const {
		return metalData->FindNearestClusters(pos, num, predicate);
	}

	const CMetalData::Clusters& GetClusters() const { return metalData->GetClusters(); }
	const CMetalData::Graph& GetGraph() const { return metalData->GetGraph(); }

public:
	void SetOpenSpot(int index, bool value);
	void SetOpenSpot(const springai::AIFloat3& pos, bool value);
	bool IsOpenSpot(int index) const { return metalInfos[index].isOpen; }
	bool IsOpenSpot(const springai::AIFloat3& pos) const;
	void MarkAllyMexes();
	void MarkAllyMexes(const std::list<CCircuitUnit*>& mexes);
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
		CCircuitUnit::Id unitId;
		int index;
	};
	std::deque<SMex> markedMexes;  // sorted by insertion
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCEMANAGER_H_
