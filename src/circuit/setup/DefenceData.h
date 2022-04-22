/*
 * DefenceData.h
 *
 *  Created on: Sep 20, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_DEFENCEDATA_H_
#define SRC_CIRCUIT_SETUP_DEFENCEDATA_H_

#include "util/Point.h"
#include "kdtree/nanoflann.hpp"

#include "AIFloat3.h"

#include <vector>

namespace circuit {

class CCircuitAI;
class CMetalManager;

class CDefenceData {
public:
	struct SDefPoint {
		springai::AIFloat3 position;
		float cost;
	};
	using DefPoints = std::vector<SDefPoint>;
	using DefIndices = std::vector<int>;
	struct SClusterInfo {
		DefIndices idxPoints;  // index of SDefPoint in defPoints array
	};

public:
	CDefenceData(CCircuitAI* circuit);
	~CDefenceData();

private:
	void ReadConfig(CCircuitAI* circuit);
	void Init(CCircuitAI* circuit);

public:
	const DefPoints& GetDefPoints() const { return defPoints; }
	const DefIndices& GetDefIndices(int cluster) const { return clusterInfos[cluster].idxPoints; }
	SDefPoint* GetDefPoint(const springai::AIFloat3& pos, float cost);

	void SetBaseRange(float range);
	float GetBaseRange() const { return baseRange; }
	float GetCommRadBegin() const { return commRadBegin; }
	float GetCommRad(float baseDist) const {
		return commRadFraction * baseDist + commRadBegin;
	}
	unsigned int GetGuardTaskNum() const { return guardTaskNum; }
	unsigned int GetGuardsNum() const { return guardsNum; }
	int GetGuardFrame() const { return guardFrame; }

private:
	CMetalManager* metalManager;
	std::vector<SClusterInfo> clusterInfos;

	DefPoints defPoints;  // starting part corresponds to choke points, rest are points in cluster
	utils::SPointAdaptor<DefPoints> defAdaptor;
	using DefTree = nanoflann::KDTreeSingleIndexAdaptor<
			nanoflann::L2_Simple_Adaptor<float, utils::SPointAdaptor<DefPoints> >,
			utils::SPointAdaptor<DefPoints>,
			2 /* dim */, int>;
	DefTree defTree;

	float baseRadMin;
	float baseRadMax;
	float baseRange;
	float commRadBegin;
	float commRadFraction;

	unsigned int guardTaskNum;
	unsigned int guardsNum;
	int guardFrame;

	float pointRange;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_DEFENCEDATA_H_
