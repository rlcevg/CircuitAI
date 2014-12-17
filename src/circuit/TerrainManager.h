/*
 * TerrainManager.h
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAINMANAGER_H_
#define SRC_CIRCUIT_TERRAINMANAGER_H_

#include "Module.h"
#include "BlockingMap.h"
#include "TerrainData.h"

#include "AIFloat3.h"

#include <unordered_map>

namespace springai {
	class UnitDef;
}

namespace circuit {

class CCircuitAI;
class CCircuitUnit;
class IBlockMask;

class CTerrainManager: public virtual IModule {
public:
	CTerrainManager(CCircuitAI* circuit);
	virtual ~CTerrainManager();

public:
	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
private:
	Handlers1 createdHandler;
	Handlers1 destroyedHandler;

public:
	int GetTerrainWidth();
	int GetTerrainHeight();
private:
	int terrainWidth;
	int terrainHeight;

public:
	// TODO: Use IsInBounds test and Bound operation only if mask or search offsets (endr) are out of bounds
	springai::AIFloat3 FindBuildSite(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);
private:
	int cacheBuildFrame;
	struct AllyBuilding {
		int unitId;
		springai::UnitDef* def;
		springai::AIFloat3 pos;
	};
	struct cmp_building {
	   bool operator()(const AllyBuilding& lhs, const AllyBuilding& rhs) {
	      return lhs.unitId < rhs.unitId;
	   }
	};
	std::map<int, AllyBuilding> markedUnits;
	void MarkAllyBuildings();

	struct SearchOffset {
		int dx, dy;
		int qdist;  // dx*dx + dy*dy
	};
	using SearchOffsets = std::vector<SearchOffset>;
	struct SearchOffsetLow {
		SearchOffsets ofs;
		int dx, dy;
		int qdist;  // dx*dx + dy*dy
	};
	using SearchOffsetsLow = std::vector<SearchOffsetLow>;
	static const SearchOffsets& GetSearchOffsetTable(int radius);
	static const SearchOffsetsLow& GetSearchOffsetTableLow(int radius);
	springai::AIFloat3 FindBuildSiteLow(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);
	springai::AIFloat3 FindBuildSiteByMask(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask);
	// NOTE: Functions using low-resolution grid are ~20% faster on fail. But ~5% slower on success.
	springai::AIFloat3 FindBuildSiteByMaskLow(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask);

	SBlockingMap blockingMap;
	std::unordered_map<springai::UnitDef*, IBlockMask*> blockInfos;  // owner
	void AddBlocker(CCircuitUnit* unit);
	void RemoveBlocker(CCircuitUnit* unit);
	void MarkBlockerByMask(CCircuitUnit* unit, bool block, IBlockMask* mask);
	void MarkBlocker(CCircuitUnit* unit, bool block);

public:
	void ClusterizeTerrain();
	const std::vector<springai::AIFloat3>& GetDefencePoints() const;
	const std::vector<springai::AIFloat3>& GetDefencePerimeter() const;
private:
	CTerrainData terrainData;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAINMANAGER_H_
