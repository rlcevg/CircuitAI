/*
 * MetalManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_METALMANAGER_H_
#define SRC_CIRCUIT_METALMANAGER_H_

#include "MetalData.h"

#include <vector>

namespace springai {
	class GameRulesParam;
	class Pathing;
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

	void ClusterLock();
	void ClusterUnlock();
	const std::vector<CMetalData::MetalIndices>& GetClusters() const;
	const std::vector<springai::AIFloat3>& GetCentroids() const;
	const std::vector<springai::AIFloat3>& GetCostCentroids() const;

private:
	CCircuitAI* circuit;
	CMetalData* metalData;

public:
	struct MetalInfo {
		bool open;
	};
	void SetOpenSpot(int index, bool value);
	const std::vector<MetalInfo>& GetMetalInfos() const;
private:
	std::vector<MetalInfo> metalInfos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_METALMANAGER_H_
