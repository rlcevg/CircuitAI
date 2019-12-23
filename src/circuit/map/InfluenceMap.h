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

#define INFL_BASE		0.f

class CMapManager;
class CAllyUnit;
class CEnemyUnit;

class CInfluenceMap {
public:
	CInfluenceMap(CMapManager* manager);
	virtual ~CInfluenceMap();

	void EnqueueUpdate();
	bool IsUpdating() const { return isUpdating; }

	float GetEnemyInflAt(const springai::AIFloat3& position) const;
	float GetInfluenceAt(const springai::AIFloat3& position) const;

	int Pos2Index(const springai::AIFloat3& pos) const;

private:
	using Influences = std::vector<float>;
	CMapManager* manager;

	void Clear();
	void AddUnit(CAllyUnit* u);
	void AddUnit(CEnemyUnit* e);
	void AddFeature(springai::Feature* f);
	inline void PosToXZ(const springai::AIFloat3& pos, int& x, int& z) const;

	void Update();
	void Apply();

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
	bool isUpdating;

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
