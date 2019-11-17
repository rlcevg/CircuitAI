/*
 * ThreatMap.h
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/ThreatMap.h
 */

#ifndef SRC_CIRCUIT_TERRAIN_THREATMAP_H_
#define SRC_CIRCUIT_TERRAIN_THREATMAP_H_

#include "unit/EnemyUnit.h"
#include "CircuitAI.h"

#include <map>
#include <vector>

namespace circuit {

#define THREAT_UPDATE_RATE	(FRAMES_PER_SEC / 4)

class CCircuitUnit;
class CEnemyUnit;

class CThreatMap {
public:
	CThreatMap(CCircuitAI* circuit, float decloakRadius);
	virtual ~CThreatMap();

	void EnqueueUpdate();

	bool EnemyEnterLOS(CEnemyUnit* enemy);
	void EnemyLeaveLOS(CEnemyUnit* enemy);
	void EnemyEnterRadar(CEnemyUnit* enemy);
	void EnemyLeaveRadar(CEnemyUnit* enemy);
	bool EnemyDestroyed(CEnemyUnit* enemy);

//	float GetAverageThreat() const { return currAvgThreat + 1.0f; }

	float GetAllThreatAt(const springai::AIFloat3& position) const;
	void SetThreatType(CCircuitUnit* unit);
	float GetThreatAt(const springai::AIFloat3& position) const;
	float GetThreatAt(CCircuitUnit* unit, const springai::AIFloat3& position) const;

	float* GetAirThreatArray() { return airThreat; }
	float* GetSurfThreatArray() { return surfThreat; }
	float* GetAmphThreatArray() { return amphThreat; }
	float* GetCloakThreatArray() { return cloakThreat; }
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
	struct SThreatData {
		FloatVec airThreat;  // air layer
		FloatVec surfThreat;  // surface (water and land)
		FloatVec amphThreat;  // under water and surface on land
		FloatVec cloakThreat;  // decloakers
		FloatVec shield;  // total shield power that covers tile
	};

	CCircuitAI* circuit;
	SAreaData* areaData;

	inline void PosToXZ(const springai::AIFloat3& pos, int& x, int& z) const;
	inline springai::AIFloat3 XZToPos(int x, int z) const;

	void Prepare(SThreatData& threatData);
	void AddEnemyUnit(const CEnemyUnit::SData& e);
	void AddEnemyUnitAll(const CEnemyUnit::SData& e);
	void AddEnemyAir(const CEnemyUnit::SData& e);  // Enemy AntiAir
	void AddEnemyAmphConst(const CEnemyUnit::SData& e);  // Enemy AntiAmph
	void AddEnemyAmphGradient(const CEnemyUnit::SData& e);  // Enemy AntiAmph
	void AddDecloaker(const CEnemyUnit::SData& e);
	void AddShield(const CEnemyUnit::SData& e);

	void SetEnemyUnitRange(CEnemyUnit* e) const;
	int GetCloakRange(const CCircuitDef* edef) const;
	int GetShieldRange(const CCircuitDef* edef) const;
	float GetEnemyUnitThreat(CEnemyUnit* enemy) const;

	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

	void Update();
	void Apply();
	SThreatData* GetNextThreatData() {
		return (pThreatData.load() == &threatData0) ? &threatData1 : &threatData0;
	}

	int squareSize;
	int width;
	int height;
	int mapSize;

	int rangeDefault;
	int distCloak;
	float slackMod;

	CCircuitAI::EnemyUnits hostileUnits;
	CCircuitAI::EnemyUnits peaceUnits;

	std::vector<CEnemyUnit::SData> hostileDatas;
	std::vector<CEnemyUnit::SData> peaceDatas;
	SThreatData threatData0, threatData1;  // Double-buffer for threading
	std::atomic<SThreatData*> pThreatData;
//	IntVec radarMap;
	IntVec sonarMap;
	IntVec losMap;
	float* drawAirThreat;
	float* drawSurfThreat;
	float* drawAmphThreat;
	float* drawCloakThreat;
	float* drawShieldArray;
	bool isUpdating;

	float* airThreat;
	float* surfThreat;
	float* amphThreat;
	float* cloakThreat;
	float* shieldArray;
	float* threatArray;  // current threat array for multiple GetThreatAt() calls

	int radarWidth;
	int radarResConv;
	int losWidth;
	int losResConv;

#ifdef DEBUG_VIS
private:
	std::vector<std::pair<uint32_t, float*>> sdlWindows;
	bool isWidgetDrawing = false;
	bool isWidgetPrinting = false;
	void UpdateVis();
public:
	void ToggleSDLVis();
	void ToggleWidgetDraw();
	void ToggleWidgetPrint();
	void SetMaxThreat(float maxThreat);
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_THREATMAP_H_
