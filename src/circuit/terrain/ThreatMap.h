/*
 * ThreatMap.h
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/ThreatMap.h
 */

#ifndef SRC_CIRCUIT_TERRAIN_THREATMAP_H_
#define SRC_CIRCUIT_TERRAIN_THREATMAP_H_

#include "unit/CircuitUnit.h"

#include <map>
#include <vector>

namespace circuit {

class CCircuitAI;
class CCircuitUnit;

class CThreatMap {
public:
	CThreatMap(CCircuitAI* circuit);
	virtual ~CThreatMap();

	void Update();

	void EnemyEnterLOS(CCircuitUnit* enemy);
	void EnemyLeaveLOS(CCircuitUnit* enemy);
	void EnemyEnterRadar(CCircuitUnit* enemy);
	void EnemyLeaveRadar(CCircuitUnit* enemy);
	void EnemyDamaged(CCircuitUnit* enemy);
	void EnemyDestroyed(CCircuitUnit* enemy);

	float GetAverageThreat() const { return currAvgThreat + 1.0f; }
	float GetThreatAt(const springai::AIFloat3&) const;

	float* GetThreatArray() { return &threatCells[0]; }
	int GetThreatMapWidth() const { return width; }
	int GetThreatMapHeight() const { return height; }

private:
	CCircuitAI* circuit;

	enum LosType: char {NONE = 0x00, LOS = 0x01, RADAR = 0x02, HIDDEN = 0x04};
	struct SEnemyUnit {
		SEnemyUnit(CCircuitUnit* enemy)
			: unit(enemy)
			, pos(-RgtVector)
			, threat(.0f)
			, range(.0f)
			, losStatus(LosType::NONE)
		{}
		CCircuitUnit* unit;
		springai::AIFloat3 pos;
		float threat;
		float range;
		std::underlying_type<LosType>::type losStatus;
	};

	void AddEnemyUnit(const SEnemyUnit& e, const float scale = 1.0f);
	void DelEnemyUnit(const SEnemyUnit& e) { AddEnemyUnit(e, -1.0f); };
	float GetEnemyUnitThreat(CCircuitUnit* enemy) const;
	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

	float currAvgThreat;
	float currMaxThreat;
	float currSumThreat;

	int width;
	int height;

	std::map<CCircuitUnit::Id, SEnemyUnit> enemyUnits;
	std::vector<float> threatCells;

//	std::vector<int> radarMap;
	std::vector<int> losMap;
//	int radarWidth;
	int losWidth;
	int losResConv;

#ifdef DEBUG_VIS
private:
	uint32_t sdlWindowId;
	float* dbgMap;
	void UpdateVis();
public:
	void ToggleVis();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_THREATMAP_H_
