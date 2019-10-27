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
	CThreatMap(CCircuitAI* circuit, float decloakRadius);
	virtual ~CThreatMap();

	const CCircuitAI::EnemyUnits& GetHostileUnits() const { return hostileUnits; }
	const CCircuitAI::EnemyUnits& GetPeaceUnits() const { return peaceUnits; }

	void Update();

	bool EnemyEnterLOS(CEnemyUnit* enemy);
	void EnemyLeaveLOS(CEnemyUnit* enemy);
	void EnemyEnterRadar(CEnemyUnit* enemy);
	void EnemyLeaveRadar(CEnemyUnit* enemy);
	void EnemyDamaged(CEnemyUnit* enemy);
	bool EnemyDestroyed(CEnemyUnit* enemy);

//	float GetAverageThreat() const { return currAvgThreat + 1.0f; }

	float GetAllThreatAt(const springai::AIFloat3& position) const;
	void SetThreatType(CCircuitUnit* unit);
	float GetThreatAt(const springai::AIFloat3& position) const;
	float GetThreatAt(CCircuitUnit* unit, const springai::AIFloat3& position) const;

	float* GetAirThreatArray() { return &airThreat[0]; }
	float* GetSurfThreatArray() { return &surfThreat[0]; }
	float* GetAmphThreatArray() { return &amphThreat[0]; }
	float* GetCloakThreatArray() { return &cloakThreat[0]; }
	int GetThreatMapWidth() const { return width; }
	int GetThreatMapHeight() const { return height; }

	float GetUnitThreat(CCircuitUnit* unit) const;
	int GetSquareSize() const { return squareSize; }
	int GetMapSize() const { return mapSize; }

private:
	/*
	 * http://stackoverflow.com/questions/872544/precision-of-floating-point
	 * Single precision: for accuracy of +/-0.5 (or 2^-1) the maximum size that the number can be is 2^23.
	 */
	using Threats = std::vector<float>;
	CCircuitAI* circuit;
	SAreaData* areaData;

	inline void PosToXZ(const springai::AIFloat3& pos, int& x, int& z) const;
	inline springai::AIFloat3 XZToPos(int x, int z) const;

	void AddEnemyUnit(const CEnemyUnit* e);
	void DelEnemyUnit(const CEnemyUnit* e);
	void AddEnemyUnitAll(const CEnemyUnit* e);
	void DelEnemyUnitAll(const CEnemyUnit* e);
	void AddEnemyAir(const CEnemyUnit* e);  // Enemy AntiAir
	void DelEnemyAir(const CEnemyUnit* e);
	void AddEnemyAmph(const CEnemyUnit* e);  // Enemy AntiAmph
	void DelEnemyAmph(const CEnemyUnit* e);
	void AddDecloaker(const CEnemyUnit* e);
	void DelDecloaker(const CEnemyUnit* e);
	void AddShield(const CEnemyUnit* e);
	void DelShield(const CEnemyUnit* e);

	void SetEnemyUnitRange(CEnemyUnit* e) const;
	int GetCloakRange(const CCircuitDef* edef) const;
	int GetShieldRange(const CCircuitDef* edef) const;
	float GetEnemyUnitThreat(CEnemyUnit* enemy) const;

	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

//	float currAvgThreat;
//	float currMaxThreat;
//	float currSumThreat;

	int squareSize;
	int width;
	int height;
	int mapSize;

	int rangeDefault;
	int distCloak;
	float slackMod;

	CCircuitAI::EnemyUnits hostileUnits;
	CCircuitAI::EnemyUnits peaceUnits;
	Threats airThreat;  // air layer
	Threats surfThreat;  // surface (water and land)
	Threats amphThreat;  // under water and surface on land
	Threats cloakThreat;
	Threats shield;
	float* threatArray;

//	std::vector<int> radarMap;
	std::vector<int> sonarMap;
	std::vector<int> losMap;
	int radarWidth;
	int radarResConv;
	int losWidth;
	int losResConv;

#ifdef DEBUG_VIS
private:
	std::vector<std::pair<uint32_t, float*>> sdlWindows;
	bool isWidgetDrawing = false;
	float maxThreat = 1.f;
	void UpdateVis();
public:
	void ToggleSDLVis();
	void ToggleWidgetVis();
	void DrawThreatAround(const springai::AIFloat3& pos);
	void SetMaxThreat(float mt) { maxThreat = mt; }
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_THREATMAP_H_
