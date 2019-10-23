/*
 * InfluenceMap.h
 *
 *  Created on: Oct 20, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_
#define SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_

#include <vector>
#ifdef DEBUG_VIS
#include <stdint.h>
#endif

namespace springai {
	class AIFloat3;
	class Feature;
}

namespace circuit {

class CCircuitAI;
class CAllyUnit;
class CEnemyUnit;

class CInfluenceMap {
public:
	CInfluenceMap(CCircuitAI* circuit);
	virtual ~CInfluenceMap();

	void Update();

	float GetEnemyInflAt(const springai::AIFloat3& position) const;
	float GetInfluenceAt(const springai::AIFloat3& position) const;

private:
	using Influences = std::vector<float>;
	CCircuitAI* circuit;

	void Clear();
	void AddUnit(CAllyUnit* u);
	void AddUnit(CEnemyUnit* e);
	void AddFeature(springai::Feature* f);
	inline void PosToXZ(const springai::AIFloat3& pos, int& x, int& z) const;

	int squareSize;
	int width;
	int height;
	int mapSize;

	float vulnMax;

	Influences enemyInfl;
	Influences allyInfl;
	Influences influence;
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