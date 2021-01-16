/*
 * DefenceMatrix.h
 *
 *  Created on: Sep 20, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_DEFENCEMATRIX_H_
#define SRC_CIRCUIT_SETUP_DEFENCEMATRIX_H_

#include "AIFloat3.h"

#include <vector>

namespace circuit {

class CCircuitAI;
class CMetalManager;

class CDefenceMatrix {
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
	CDefenceMatrix(CCircuitAI* circuit);
	virtual ~CDefenceMatrix();

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
	unsigned int GetDefendTaskNum() const { return defendTaskNum; }
	unsigned int GetDefendersNum() const { return defendersNum; }
	int GetDefendFrame() const { return defendFrame; }

private:
	CMetalManager* metalManager;
	std::vector<SClusterInfo> clusterInfos;

	float baseRadMin;
	float baseRadMax;
	float baseRange;
	float commRadBegin;
	float commRadFraction;

	unsigned int defendTaskNum;
	unsigned int defendersNum;
	int defendFrame;

	float pointRange;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_DEFENCEMATRIX_H_
