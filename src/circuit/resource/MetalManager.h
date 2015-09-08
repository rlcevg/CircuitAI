/*
 * ResourceManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCEMANAGER_H_
#define SRC_CIRCUIT_RESOURCEMANAGER_H_

#include "static/MetalData.h"
#include "unit/CircuitUnit.h"

namespace springai {
	class GameRulesParam;
}

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
	void ParseMetalSpots(const std::vector<springai::GameRulesParam*>& metalParams);

	bool HasMetalSpots();
	bool HasMetalClusters();
	bool IsClusterizing();

	void ClusterizeMetal();
	void Init();

public:
	const CMetalData::Metals& GetSpots() const;
	const int FindNearestSpot(const springai::AIFloat3& pos) const;
	const int FindNearestSpot(const springai::AIFloat3& pos, CMetalData::MetalPredicate& predicate) const;
	const CMetalData::MetalIndices FindNearestSpots(const springai::AIFloat3& pos, int num) const;
	const CMetalData::MetalIndices FindNearestSpots(const springai::AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const;
	const CMetalData::MetalIndices FindWithinDistanceSpots(const springai::AIFloat3& pos, float maxDistance) const;
	const CMetalData::MetalIndices FindWithinRangeSpots(const springai::AIFloat3& posFrom, const springai::AIFloat3& posTo) const;

	const int FindNearestCluster(const springai::AIFloat3& pos) const;
	const int FindNearestCluster(const springai::AIFloat3& pos, CMetalData::MetalPredicate& predicate) const;
	const CMetalData::MetalIndices FindNearestClusters(const springai::AIFloat3& pos, int num) const;
	const CMetalData::MetalIndices FindNearestClusters(const springai::AIFloat3& pos, int num, CMetalData::MetalPredicate& predicate) const;

	const CMetalData::Clusters& GetClusters() const;
	const CMetalData::Graph& GetGraph() const;

public:
	void SetOpenSpot(int index, bool value);
	void SetOpenSpot(const springai::AIFloat3& pos, bool value);
	bool IsOpenSpot(int index);
	void MarkAllyMexes();
	void MarkAllyMexes(const std::list<CCircuitUnit*>& mexes);
	bool IsClusterOur(int index);

private:
	CCircuitAI* circuit;
	CMetalData* metalData;

	struct SMetalInfo {
		bool isOpen;
		int clusterId;
	};
	struct SClusterInfo {
		int mexCount;
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
