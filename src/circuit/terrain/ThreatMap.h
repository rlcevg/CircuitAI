/*
 * ThreatMap.h
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/ThreatMap.h
 */

#ifndef SRC_CIRCUIT_TERRAIN_THREATMAP_H_
#define SRC_CIRCUIT_TERRAIN_THREATMAP_H_

#include "CircuitAI.h"

#include <map>
#include <vector>

namespace circuit {

class CCircuitUnit;
class CEnemyUnit;

class CThreatMap {
public:
	CThreatMap(CCircuitAI* circuit);
	virtual ~CThreatMap();

	const CCircuitAI::EnemyUnits& GetHostileUnits() const { return hostileUnits; }
	const CCircuitAI::EnemyUnits& GetPeaceUnits() const { return peaceUnits; }

	void Update();

	void EnemyEnterLOS(CEnemyUnit* enemy);
	void EnemyLeaveLOS(CEnemyUnit* enemy);
	void EnemyEnterRadar(CEnemyUnit* enemy);
	void EnemyLeaveRadar(CEnemyUnit* enemy);
	void EnemyDamaged(CEnemyUnit* enemy);
	void EnemyDestroyed(CEnemyUnit* enemy);

	float GetAverageThreat() const { return currAvgThreat + 1.0f; }
	float GetThreatAt(const springai::AIFloat3&) const;

	float* GetThreatArray() { return &threatCells[0]; }
	float* GetThreatCloakArray() { return &threatCloak[0]; }
	int GetThreatMapWidth() const { return width; }
	int GetThreatMapHeight() const { return height; }

	float GetUnitThreat(CCircuitUnit* unit) const;

private:
	CCircuitAI* circuit;

	void AddEnemyUnit(const CEnemyUnit* e, const float scale = 1.0f);
	void DelEnemyUnit(const CEnemyUnit* e) { AddEnemyUnit(e, -1.0f); };
	float GetEnemyUnitThreat(CEnemyUnit* enemy) const;
	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

	float currAvgThreat;
	float currMaxThreat;
	float currSumThreat;

	int squareSize;
	int width;
	int height;

	int rangeDefault;
	int rangeCloakSq;

	CCircuitAI::EnemyUnits hostileUnits;
	CCircuitAI::EnemyUnits peaceUnits;
	std::vector<float> threatCells;
	std::vector<float> threatCloak;

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
