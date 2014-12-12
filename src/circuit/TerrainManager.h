/*
 * TerrainManager.h
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAINMANAGER_H_
#define SRC_CIRCUIT_TERRAINMANAGER_H_

#include "Module.h"

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

private:
//	static springai::AIFloat3 Pos2BuildPos(int xsize, int zsize, const springai::AIFloat3& pos);
	struct SearchOffset {
		int dx, dy;
		int qdist;  // dx*dx + dy*dy
	};
	static const std::vector<SearchOffset>& GetSearchOffsetTable(int radius);
	springai::AIFloat3 FindBuildSiteByMask(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask);
public:
	springai::AIFloat3 FindBuildSite(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);

private:
	Handlers1 createdHandler;
	Handlers1 destroyedHandler;

	int terrainWidth;
	int terrainHeight;

	struct BlockingMap {
		std::vector<int> grid;
		int cellRows;
		inline bool IsStruct(int x, int z);
		inline bool IsBlocked(int x, int z);
		inline void MarkBlocker(int x, int z);
		inline void AddBlocker(int x, int z, int count);
		inline void AddStruct(int x, int z);
		inline void RemoveStruct(int x, int z);
	} blockingMap;
	std::unordered_map<springai::UnitDef*, IBlockMask*> blockInfos;  // owner
	void AddBlocker(CCircuitUnit* unit);
	void RemoveBlocker(CCircuitUnit* unit);
	void MarkBlockerByMask(CCircuitUnit* unit, int count, IBlockMask* mask);
	void MarkBlocker(CCircuitUnit* unit, int count);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAINMANAGER_H_
