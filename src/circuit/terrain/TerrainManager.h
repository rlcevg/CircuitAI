/*
 * TerrainManager.h
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_TERRAINMANAGER_H_
#define SRC_CIRCUIT_TERRAIN_TERRAINMANAGER_H_

#include "terrain/BlockingMap.h"

#include "AIFloat3.h"

#include <unordered_map>
#include <set>

namespace springai {
	class UnitDef;
}

namespace circuit {

class CCircuitAI;
class CCircuitUnit;
class IBlockMask;
class CTerrainData;

class CTerrainManager {
public:
	CTerrainManager(CCircuitAI* circuit, CTerrainData* terrainData);
	virtual ~CTerrainManager();
private:
	CCircuitAI* circuit;

public:
	int GetTerrainWidth();
	int GetTerrainHeight();
private:
	int terrainWidth;
	int terrainHeight;

public:
	void AddBlocker(springai::UnitDef* unitDef, const springai::AIFloat3& pos, int facing);
	void RemoveBlocker(springai::UnitDef* unitDef, const springai::AIFloat3& pos, int facing);
	// TODO: Use IsInBounds test and Bound operation only if mask or search offsets (endr) are out of bounds
	// TODO: Use A* to calculate build offset
	springai::AIFloat3 FindBuildSite(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);
private:
	int cacheBuildFrame;
	struct Structure {
		int unitId;
		springai::UnitDef* def;
		springai::AIFloat3 pos;
		int facing;
	};
	struct cmp {
	   bool operator()(const Structure& lhs, const Structure& rhs) {
	      return lhs.unitId < rhs.unitId;
	   }
	};
	std::set<Structure, cmp> markedAllies;
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
	// NOTE: Low-resolution build site is 40-80% faster on fail and 20-50% faster on success (with large objects). But has lower precision.
	springai::AIFloat3 FindBuildSiteByMaskLow(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask);

	SBlockingMap blockingMap;
	std::unordered_map<springai::UnitDef*, IBlockMask*> blockInfos;  // owner
	void MarkBlockerByMask(const Structure& building, bool block, IBlockMask* mask);
	void MarkBlocker(const Structure& building, bool block);

public:
	void ClusterizeTerrain();
	const std::vector<springai::AIFloat3>& GetDefencePoints() const;
	const std::vector<springai::AIFloat3>& GetDefencePerimeter() const;
private:
	CTerrainData* terrainData;
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_TERRAINMANAGER_H_
