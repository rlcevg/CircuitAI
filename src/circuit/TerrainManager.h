/*
 * TerrainManager.h
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAINMANAGER_H_
#define SRC_CIRCUIT_TERRAINMANAGER_H_

#include "Module.h"

#include "System/type2.h"
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
	void Init();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	int GetTerrainWidth();
	int GetTerrainHeight();

public:
	springai::AIFloat3 FindBuildSite(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);
private:
//	static springai::AIFloat3 Pos2BuildPos(int xsize, int zsize, const springai::AIFloat3& pos);
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
	springai::AIFloat3 FindBuildSiteByMaskLow(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask);

private:
	Handlers1 createdHandler;
	Handlers1 destroyedHandler;

	int terrainWidth;
	int terrainHeight;

	struct BlockingMap {
		std::vector<int> grid;     // granularity Map::GetWidth / 2,  Map::GetHeight / 2
		int columns;
		int rows;
		std::vector<int> gridLow;  // granularity Map::GetWidth / 16, Map::GetHeight / 16
		int columnsLow;
		int rowsLow;
		inline bool IsStruct(int x, int z);
		inline bool IsBlocked(int x, int z);
		inline bool IsBlockedLow(int x, int z);
		inline void MarkBlocker(int x, int z);
		inline void AddBlocker(int x, int z);
		inline void RemoveBlocker(int x, int z);
		inline void AddStruct(int x, int z);
		inline void RemoveStruct(int x, int z);

		inline bool IsInBounds(const int2& r1, const int2& r2);
		inline bool IsInBoundsLow(int x, int z);
		inline void Bound(int2& r1, int2& r2);
	} blockingMap;
	std::unordered_map<springai::UnitDef*, IBlockMask*> blockInfos;  // owner
	void AddBlocker(CCircuitUnit* unit);
	void RemoveBlocker(CCircuitUnit* unit);
	void MarkBlockerByMask(CCircuitUnit* unit, bool block, IBlockMask* mask);
	void MarkBlocker(CCircuitUnit* unit, bool block);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAINMANAGER_H_
