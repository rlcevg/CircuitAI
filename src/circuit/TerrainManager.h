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

#include <vector>
#include <unordered_map>

namespace springai {
	class UnitDef;
}

namespace circuit {

class CCircuitAI;
class CCircuitUnit;

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
	springai::AIFloat3 Pos2BuildPos(int xsize, int zsize, const springai::AIFloat3& pos);
	struct SearchOffset {
		int dx, dy;
		int qdist;  // dx*dx + dy*dy
	};
	static const std::vector<SearchOffset>& GetSearchOffsetTable(int radius);
public:
	springai::AIFloat3 FindBuildSite(springai::UnitDef* unitDef, const springai::AIFloat3& pos, float searchRadius, int facing);

private:
	Handlers1 createdHandler;
	Handlers1 destroyedHandler;

	int terrainWidth;
	int terrainHeight;

	int cellRows;
	std::vector<int> blockingMap;
	struct BlockInfo {
		// TODO: yard-map alternative
		int xsize;  // UnitDef::GetXSize() / 2
		int zsize;  // UnitDef::GetZSize() / 2
		springai::AIFloat3 offset;
	};
	std::unordered_map<springai::UnitDef*, BlockInfo> blockInfo;
	void AddBlocker(CCircuitUnit* unit);
	void RemoveBlocker(CCircuitUnit* unit);
	void MarkBlocker(CCircuitUnit* unit, int count);
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAINMANAGER_H_
