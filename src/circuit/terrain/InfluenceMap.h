/*
 * InfluenceMap.h
 *
 *  Created on: Oct 20, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_
#define SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_

#include "CircuitAI.h"

#include <vector>

namespace circuit {

class CInfluenceMap {
public:
	CInfluenceMap(CCircuitAI* circuit);
	virtual ~CInfluenceMap();

	void Update();

	void Clear() {
		std::fill(enemyInfl.begin(), enemyInfl.end(), INFL_BASE);
		std::fill(allyInfl.begin(), allyInfl.end(), INFL_BASE);
		std::fill(influence.begin(), influence.end(), INFL_BASE);
		std::fill(influenceCost.begin(), influenceCost.end(), THREAT_BASE);
		std::fill(tension.begin(), tension.end(), INFL_BASE);
		std::fill(vulnerability.begin(), vulnerability.end(), INFL_BASE);
		std::fill(featureInfl.begin(), featureInfl.end(), INFL_BASE);
	}
	void AddUnit(CCircuitUnit* u);
//	void DelUnit(CCircuitUnit* u);
	void AddUnit(CEnemyUnit* e);
//	void DelUnit(CEnemyUnit* e);
	void AddFeature(springai::Feature* f);

	float* GetInfluenceCostArray() { return influenceCost.data(); }

private:
	using Influences = std::vector<float>;
	CCircuitAI* circuit;

	inline void PosToXZ(const springai::AIFloat3& pos, int& x, int& z) const;

	int squareSize;
	int width;
	int height;
	int mapSize;

	float inflCostMax;
	float vulnMaxInfl;

	//	CCircuitAI::EnemyUnits hostileUnits;
//	CCircuitAI::EnemyUnits peaceUnits;
	Influences enemyInfl;
	Influences allyInfl;
	Influences influence;
	Influences influenceCost;
	Influences tension;
	Influences vulnerability;
	Influences featureInfl;

#ifdef DEBUG_VIS
private:
	std::vector<std::pair<uint32_t, float*>> sdlWindows;
	void UpdateVis();
public:
	void ToggleVis();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_
