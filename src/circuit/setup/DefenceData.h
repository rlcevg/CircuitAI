/*
 * DefenceData.h
 *
 *  Created on: Sep 20, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_DEFENCEDATA_H_
#define SRC_CIRCUIT_SETUP_DEFENCEDATA_H_

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
	struct SClusterInfo {
		DefPoints defPoints;
	};

public:
	CDefenceData(CCircuitAI* circuit);
	virtual ~CDefenceData();

private:
	void ReadConfig(CCircuitAI* circuit);
	void Init(CCircuitAI* circuit);

public:
	std::vector<SDefPoint>& GetDefPoints(int index) { return clusterInfos[index].defPoints; }
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
