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

//	float GetAverageThreat() const { return currAvgThreat + 1.0f; }
	float GetAllThreatAt(const springai::AIFloat3& position) const;
	void SetThreatType(CCircuitUnit* unit);
	float GetThreatAt(const springai::AIFloat3& position) const;
	float GetThreatAt(CCircuitUnit* unit, const springai::AIFloat3& position) const;

	float* GetAirThreatArray() { return &airThreat[0]; }
	float* GetLandThreatArray() { return &landThreat[0]; }
	float* GetWaterThreatArray() { return &waterThreat[0]; }
	float* GetCloakThreatArray() { return &cloakThreat[0]; }
	int GetThreatMapWidth() const { return width; }
	int GetThreatMapHeight() const { return height; }

	float GetUnitThreat(CCircuitUnit* unit) const;
	int GetSquareSize() const { return squareSize; }

private:
	using Threats = std::vector<float>;
	CCircuitAI* circuit;

	void AddEnemyUnit(const CEnemyUnit* e, const float scale = 1.0f);
	void DelEnemyUnit(const CEnemyUnit* e) { AddEnemyUnit(e, -1.0f); };
	void AddEnemyUnitAll(const CEnemyUnit* e, const float scale = 1.0f);
	void DelEnemyUnitAll(const CEnemyUnit* e) { AddEnemyUnitAll(e, -1.0f); };
	void AddEnemyUnit(const CEnemyUnit* e, Threats& threats, const float scale = 1.0f);
	void DelEnemyUnit(const CEnemyUnit* e, Threats& threats) { AddEnemyUnit(e, threats, -1.0f); };
	void AddDecloaker(const CEnemyUnit* e, const float scale = 1.0f);
	void DelDecloaker(const CEnemyUnit* e) { AddDecloaker(e, -1.0f); };

	int GetEnemyUnitRange(const CEnemyUnit* e) const;
	int GetCloakRange(const CEnemyUnit* e) const;
	float GetEnemyUnitThreat(CEnemyUnit* enemy) const;

	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

//	float currAvgThreat;
//	float currMaxThreat;
//	float currSumThreat;

	int squareSize;
	int width;
	int height;

	int rangeDefault;
	int distCloak;

	CCircuitAI::EnemyUnits hostileUnits;
	CCircuitAI::EnemyUnits peaceUnits;
	Threats airThreat;
	Threats landThreat;  // surface
	Threats waterThreat;  // under water
	Threats cloakThreat;
	float* threatArray;

//	std::vector<int> radarMap;
	std::vector<int> losMap;
//	int radarWidth;
	int losWidth;
	int losResConv;

#ifdef DEBUG_VIS
private:
	std::vector<std::pair<uint32_t, float*>> sdlWindows;
	void UpdateVis();
public:
	void ToggleVis();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_THREATMAP_H_
