/*
 * InfluenceMap.h
 *
 *  Created on: Oct 20, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_
#define SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_

#include "unit/enemy/EnemyUnit.h"

#include <vector>
#ifdef DEBUG_VIS
#include <stdint.h>
#endif

namespace springai {
	class AIFloat3;
	class Feature;
}

namespace circuit {

#define INFL_BASE		0.f
#define INFL_EPS		0.01f
#define INFL_SAFE		2.f

class CEnemyManager;
class CMapManager;
class CAllyUnit;
class IMainJob;

class CInfluenceMap {
public:
	CInfluenceMap(CMapManager* manager);
	~CInfluenceMap();

private:
	void ReadConfig();

public:
	void EnqueueUpdate();
	bool IsUpdating() const { return isUpdating; }

	float GetEnemyInflAt(const springai::AIFloat3& position) const;
	float GetAllyInflAt(const springai::AIFloat3& position) const;
	float GetAllyDefendInflAt(const springai::AIFloat3& position) const;
	float GetInfluenceAt(const springai::AIFloat3& position) const;

	int Pos2Index(const springai::AIFloat3& pos) const;

private:
	struct SInfluenceData {
		FloatVec enemyInfl;
		FloatVec allyInfl;
		FloatVec allyDefendInfl;
		FloatVec influence;
//		FloatVec tension;
//		FloatVec vulnerability;
//		FloatVec featureInfl;
	};

	CMapManager* manager;

	int GetUnitRange(CAllyUnit* u) const;

	void AddMobileArmed(CAllyUnit* u);
	void AddStaticArmed(CAllyUnit* u);
	void AddUnarmed(CAllyUnit* u);
	void AddEnemy(const SEnemyData& e);
//	void AddFeature(springai::Feature* f);
	inline void PosToXZ(const springai::AIFloat3& pos, int& x, int& z) const;

	void Prepare(SInfluenceData& inflData);
	std::shared_ptr<IMainJob> Update(CEnemyManager* enemyMgr);
	void Apply();
	void SwapBuffers();
	SInfluenceData* GetNextInflData() {
		return (pInflData.load() == &inflData0) ? &inflData1 : &inflData0;
	}

	int squareSize;
	int width;
	int height;
	int mapSize;

//	float vulnMax;

	SInfluenceData inflData0, inflData1;  // Double-buffer for threading
	std::atomic<SInfluenceData*> pInflData;
	float* drawEnemyInfl;
	float* drawAllyInfl;
	float* drawAllyDefendInfl;
	float* drawInfluence;
//	float* drawTension;
//	float* drawVulnerability;
//	float* drawFeatureInfl;
	bool isUpdating;

	float* enemyInfl;
	float* allyInfl;
	float* allyDefendInfl;
	float* influence;
//	float* tension;
//	float* vulnerability;
//	float* featureInfl;

	float defRadius;

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

#endif // SRC_CIRCUIT_TERRAIN_INFLUENCEMAP_H_
