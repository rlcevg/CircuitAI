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
	float GetAirMetal()    const { return airMetal; }
	float GetStaticMetal() const { return staticMetal; }
	float GetLandMetal()   const { return landMetal; }
	float GetWaterMetal()  const { return waterMetal; }

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

	void AddEnemyUnit(const CEnemyUnit* e, const float scale = 1.0f);
	void DelEnemyUnit(const CEnemyUnit* e) { AddEnemyUnit(e, -1.0f); }
	void AddEnemyUnitAll(const CEnemyUnit* e, const float scale = 1.0f);
	void DelEnemyUnitAll(const CEnemyUnit* e) { AddEnemyUnitAll(e, -1.0f); }
	void AddEnemyAir(const CEnemyUnit* e, const float scale = 1.0f);  // Enemy AntiAir
	void DelEnemyAir(const CEnemyUnit* e) { AddEnemyAir(e, -1.0f); }
	void AddEnemyAmph(const CEnemyUnit* e, const float scale = 1.0f);  // Enemy AntiAmph
	void DelEnemyAmph(const CEnemyUnit* e) { AddEnemyAmph(e, -1.0f); }
	void AddDecloaker(const CEnemyUnit* e, const float scale = 1.0f);
	void DelDecloaker(const CEnemyUnit* e) { AddDecloaker(e, -1.0f); }
	void AddEnemyMetal(const CEnemyUnit* e, const float scale = 1.0f);
	void DelEnemyMetal(const CEnemyUnit* e) { AddEnemyMetal(e, -1.0f); }

	void SetEnemyUnitRange(CEnemyUnit* e) const;
	int GetCloakRange(const CEnemyUnit* e) const;
	float GetEnemyUnitThreat(CEnemyUnit* enemy) const;

	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

//	float currAvgThreat;
//	float currMaxThreat;
//	float currSumThreat;
	float airMetal;
	float staticMetal;
	float landMetal;
	float waterMetal;

	int squareSize;
	int width;
	int height;
	int mapSize;

	int rangeDefault;
	int distCloak;

	CCircuitAI::EnemyUnits hostileUnits;
	CCircuitAI::EnemyUnits peaceUnits;
	Threats airThreat;  // air layer
	Threats surfThreat;  // surface (water and land)
	Threats amphThreat;  // under water and surface on land
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
