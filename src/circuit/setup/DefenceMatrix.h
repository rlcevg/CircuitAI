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

	float GetBaseRange() const { return baseRange; }
	float GetCommRad(float baseDist) const {
		return commRadFraction * baseDist + commRadBegin;
	}

private:
	CMetalManager* metalManager;
	std::vector<SClusterInfo> clusterInfos;

	float baseRange;
	float commRadFraction;
	float commRadBegin;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_DEFENCEMATRIX_H_
