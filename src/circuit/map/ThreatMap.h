/*
 * ThreatMap.h
 *
 *  Created on: Aug 25, 2015
 *      Author: rlcevg
 *      Original implementation: https://github.com/spring/KAIK/blob/master/ThreatMap.h
 */

#ifndef SRC_CIRCUIT_TERRAIN_THREATMAP_H_
#define SRC_CIRCUIT_TERRAIN_THREATMAP_H_

#include "unit/enemy/EnemyUnit.h"

#include <map>
#include <vector>

//#define CHRONO_THREAT 1
#ifdef CHRONO_THREAT
#include <chrono>
#endif

namespace circuit {

#define THREAT_UPDATE_RATE	(FRAMES_PER_SEC / 3)
#define THREAT_BASE			0.f

class CEnemyManager;
class CMapManager;
class CCircuitUnit;
class IMainJob;

class CThreatMap {
public:
#ifdef CHRONO_THREAT
	using clock = std::chrono::high_resolution_clock;
	clock::time_point t0;
#endif
	CThreatMap(CMapManager* manager, float decloakRadius);
	virtual ~CThreatMap();

	void Init(const int roleSize, std::set<CCircuitDef::RoleT>&& modRoles);

	void EnqueueUpdate();
	bool IsUpdating() const { return isUpdating; }

	void SetEnemyUnitRange(CEnemyUnit* e) const;
	void SetEnemyUnitThreat(CEnemyUnit* e) const;
	void NewEnemy(CEnemyUnit* e) const;

	float GetBuilderThreatAt(const springai::AIFloat3& position) const;
	void SetThreatType(CCircuitUnit* unit);
	float GetThreatAt(const springai::AIFloat3& position) const;
	float GetThreatAt(CCircuitUnit* unit, const springai::AIFloat3& position) const;

	float* GetAirThreatArray(CCircuitDef::RoleT type) { return pThreatData.load()->roleThreatPtrs[type]->airThreat.data(); }
	float* GetSurfThreatArray(CCircuitDef::RoleT type) { return pThreatData.load()->roleThreatPtrs[type]->surfThreat.data(); }
	float* GetAmphThreatArray(CCircuitDef::RoleT type) { return pThreatData.load()->roleThreatPtrs[type]->amphThreat.data(); }
	float* GetSwimThreatArray(CCircuitDef::RoleT type) { return pThreatData.load()->roleThreatPtrs[type]->swimThreat.data(); }
	float* GetCloakThreatArray() { return cloakThreat; }
	int GetThreatMapWidth() const { return width; }
	int GetThreatMapHeight() const { return height; }

	float GetUnitPower(CCircuitUnit* unit) const;
	int GetSquareSize() const { return squareSize; }
	int GetMapSize() const { return mapSize; }

private:
	/*
	 * http://stackoverflow.com/questions/872544/precision-of-floating-point
	 * Single precision: for accuracy of +/-0.5 (or 2^-1) the maximum size that the number can be is 2^23.
	 */
	struct SRoleThreat {
		FloatVec airThreat;  // air layer
		FloatVec surfThreat;  // surface (water and land)
		FloatVec amphThreat;  // under water and surface on land
		FloatVec swimThreat;  // under water and on water
	};
	struct SThreatData {
		std::map<CCircuitDef::RoleT, SRoleThreat> roleThreats;
		std::vector<SRoleThreat*> roleThreatPtrs;
		SRoleThreat* defThreat;  // default when role is not modded
		FloatVec cloakThreat;  // decloakers
		FloatVec shield;  // total shield power that covers tile
	};

	CMapManager* manager;
	terrain::SAreaData* areaData;

	inline void PosToXZ(const springai::AIFloat3& pos, int& x, int& z) const;
	inline springai::AIFloat3 XZToPos(int x, int z) const;

	void AddEnemyUnit(SEnemyData& e);
	void AddEnemyAir(const float threat, float* drawAirThreat,
			const SEnemyData& e, const int slack = 0);  // Enemy AntiAir
	void AddEnemyAmphConst(const float threatLand, const float threatWater,
			float* drawSurfThreat, float* drawAmphThreat, float* drawSwimThreat,
			const SEnemyData& e, const int slack = 0);  // Enemy AntiAmph
	void AddEnemyAmphGradient(const float threatLand, const float threatWater,
			float* drawSurfThreat, float* drawAmphThreat, float* drawSwimThreat,
			const SEnemyData& e, const int slack = 0);  // Enemy AntiAmph
	void AddDecloaker(float* drawCloakThreat, const SEnemyData& e);
	void AddShield(float* drawShieldArray, const SEnemyData& e);

	int GetCloakRange(const CCircuitDef* edef) const;
	int GetShieldRange(const CCircuitDef* edef) const;
	float GetThreatHealth(const CEnemyUnit* e) const;

	std::shared_ptr<IMainJob> Update(CEnemyManager* enemyMgr, CScheduler* scheduler);
	std::shared_ptr<IMainJob> AirDrawer(CCircuitDef::RoleT role);
	std::shared_ptr<IMainJob> AmphDrawer(CCircuitDef::RoleT role);
	std::shared_ptr<IMainJob> ApplyDrawers();
	void Apply();
	void SwapBuffers();
	SThreatData* GetNextThreatData() {
		return (pThreatData.load() == &threatData0) ? &threatData1 : &threatData0;
	}

	std::vector<const SEnemyData*> airDraws;
	std::vector<const SEnemyData*> amphDraws;
	std::atomic<int> numThreadDraws;

	int squareSize;
	int width;
	int height;
	int mapSize;

	int rangeDefault;
	int distCloak;
	struct {
		float allMod;
		float staticMod;
		float speedMod;
		int speedModMax;
	} slackMod;

	SThreatData threatData0, threatData1;  // Double-buffer for threading
	std::atomic<SThreatData*> pThreatData;
	CCircuitDef::RoleT defRole; // represents non-modded roles
	bool isUpdating;

	float* cloakThreat;
	float* shieldArray;
	float* threatArray;  // current threat array for multiple GetThreatAt() calls

#ifdef DEBUG_VIS
private:
	std::vector<std::pair<uint32_t, float*>> sdlWindows;
	bool isWidgetDrawing = false;
	bool isWidgetPrinting = false;
	int layerDbg = 1;
	void UpdateVis();
public:
	void ToggleSDLVis();
	void ToggleWidgetDraw();
	void ToggleWidgetPrint();
	void SetMaxThreat(float maxThreat, std::string layer);
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_THREATMAP_H_
