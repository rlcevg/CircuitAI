/*
 * TerrainManager.h
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TERRAIN_TERRAINMANAGER_H_
#define SRC_CIRCUIT_TERRAIN_TERRAINMANAGER_H_

#include "terrain/BlockingMap.h"
#include "unit/CoreUnit.h"
#include "unit/CircuitDef.h"

#include "AIFloat3.h"

#include <unordered_map>
#include <deque>
#include <functional>

namespace terrain {
	struct SArea;
	struct SMobileType;
	struct SImmobileType;
	struct SAreaSector;
	struct SSector;
	struct SAreaData;
}

namespace circuit {

class CCircuitAI;
class IBlockMask;
class IPathQuery;
class CPathInfo;
class CCircuitUnit;

class CTerrainManager final {  // <=> RAI's cBuilderPlacement
public:
	using TerrainPredicate = std::function<bool (const springai::AIFloat3& p)>;

	CTerrainManager(CCircuitAI* circuit, terrain::CTerrainData* terrainData);
	~CTerrainManager();
private:
	CCircuitAI* circuit;
	void ReadConfig();

public:
	static inline int GetTerrainWidth() { return springai::AIFloat3::maxxpos; }
	static inline int GetTerrainHeight() { return springai::AIFloat3::maxzpos; }
	static inline int GetTerrainDiagonal() {
		return sqrtf(SQUARE(springai::AIFloat3::maxxpos) + SQUARE(springai::AIFloat3::maxzpos));
	}
	static inline springai::AIFloat3 GetTerrainCenter() {
		return springai::AIFloat3(GetTerrainWidth() / 2, 0, GetTerrainHeight() / 2);
	}

public:
	void Init();
	void AddBlocker(CCircuitDef* cdef, const springai::AIFloat3& pos, int facing, bool isOffset = false);
	void DelBlocker(CCircuitDef* cdef, const springai::AIFloat3& pos, int facing, bool isOffset = false);
	void AddBusPath(CCircuitUnit* unit, const springai::AIFloat3& pos, const CCircuitDef* mobileDef);
	void DelBusPath(CCircuitUnit* unit);
	void ResetBuildFrame() { markFrame = -FRAMES_PER_SEC; }
	// TODO: Use IsInBounds test and Bound operation only if mask or search offsets (endr) are out of bounds
	// TODO: Based on map complexity use BFS or circle to calculate build offset
	// TODO: Consider abstract task position (any area with builder) and task for certain unit-pos-area
	springai::AIFloat3 FindBuildSite(CCircuitDef* cdef,
									 const springai::AIFloat3& pos,
									 float searchRadius,
									 int facing,
									 bool isIgnore = false);
	springai::AIFloat3 FindBuildSite(CCircuitDef* cdef,
									 const springai::AIFloat3& pos,
									 float searchRadius,
									 int facing,
									 TerrainPredicate& predicate,
									 bool isIgnore = false);
//	springai::AIFloat3 FindSpringBuildSite(CCircuitDef* cdef, const springai::AIFloat3& pos, float searchRadius, int facing);
	void DoLineOfDef(const springai::AIFloat3& start, const springai::AIFloat3& end, CCircuitDef* buildDef,
			std::function<void (const springai::AIFloat3& pos, CCircuitDef* buildDef)> exec) const;  // FillRowOfBuildPos

	const SBlockingMap& GetBlockingMap();

	bool ResignAllyBuilding(CCircuitUnit* unit);

private:
	int markFrame;
	struct SStructure {
		ICoreUnit::Id unitId;
		CCircuitDef* cdef;
		springai::AIFloat3 pos;
		int facing;
	};
	std::deque<SStructure> markedAllies;  // sorted by insertion
	void MarkAllyBuildings();

	struct SSearchOffset {
		int dx, dy;
		int qdist;  // dx*dx + dy*dy
	};
	using SearchOffsets = std::vector<SSearchOffset>;
	struct SSearchOffsetLow {
		SearchOffsets ofs;
		int dx, dy;
		int qdist;  // dx*dx + dy*dy
	};
	using SearchOffsetsLow = std::vector<SSearchOffsetLow>;
	static const SearchOffsets& GetSearchOffsetTable(int radius);
	static const SearchOffsetsLow& GetSearchOffsetTableLow(int radius);
	springai::AIFloat3 FindBuildSiteLow(CCircuitDef* cdef,
										const springai::AIFloat3& pos,
										float searchRadius,
										int facing,
										TerrainPredicate& predicate);
	springai::AIFloat3 FindBuildSiteByMask(CCircuitDef* cdef,
										   const springai::AIFloat3& pos,
										   float searchRadius,
										   int facing,
										   IBlockMask* mask,
										   TerrainPredicate& predicate);
	// NOTE: Low-resolution build site is 40-80% faster on fail and 20-50% faster on success (with large objects). But has lower precision.
	springai::AIFloat3 FindBuildSiteByMaskLow(CCircuitDef* cdef,
											  const springai::AIFloat3& pos,
											  float searchRadius,
											  int facing,
											  IBlockMask* mask,
											  TerrainPredicate& predicate);

	SBlockingMap blockingMap;
	std::unordered_map<CCircuitDef::Id, IBlockMask*> blockInfos;  // owner
	void MarkBlockerByMask(const SStructure& building, bool block, IBlockMask* mask);
	void MarkBlocker(const SStructure& building, bool block);

	struct FactoryPathQuery {
		std::shared_ptr<IPathQuery> query;
		const CCircuitDef* mobileDef;
		springai::AIFloat3 startPos;
		springai::AIFloat3 endPos;
		IndexVec targets;
	};
	std::map<CCircuitUnit*, std::shared_ptr<CPathInfo>> busPath;
	std::map<CCircuitUnit*, FactoryPathQuery> busQueries;
	void MarkBusPath();

public:
	int GetConvertStoP() const { return terrainData->convertStoP; }
	int GetSectorXSize() const { return terrainData->sectorXSize; }
	int GetSectorZSize() const { return terrainData->sectorZSize; }
	static void CorrectPosition(springai::AIFloat3& position) { terrain::CTerrainData::CorrectPosition(position); }
	static springai::AIFloat3 CorrectPosition(const springai::AIFloat3& pos, const springai::AIFloat3& dir, float& len) {
		return terrain::CTerrainData::CorrectPosition(pos, dir, len);
	}
	static void SnapPosition(springai::AIFloat3& position);
	std::pair<terrain::SArea*, bool> GetCurrentMapArea(CCircuitDef* cdef, const springai::AIFloat3& position);
	std::pair<terrain::SArea*, bool> GetCurrentMapArea(CCircuitDef* cdef, const int indexSector);
	int GetSectorIndex(const springai::AIFloat3& position) const { return terrainData->GetSectorIndex(position); }
	bool CanMoveToPos(terrain::SArea* area, const springai::AIFloat3& destination);
	springai::AIFloat3 GetBuildPosition(CCircuitDef* cdef, const springai::AIFloat3& position);
	springai::AIFloat3 GetMovePosition(terrain::SArea* sourceArea, const springai::AIFloat3& position);
private:
	std::vector<terrain::SAreaSector>& GetSectorList(terrain::SArea* sourceArea = nullptr);
	terrain::SAreaSector* GetClosestSectorWithAltitude(terrain::SArea* sourceArea, const int destinationSIndex, const int altitude);
	terrain::SAreaSector* GetClosestSector(terrain::SArea* sourceArea, const int destinationSIndex);
	terrain::SSector* GetClosestSector(terrain::SImmobileType* sourceIT, const int destinationSIndex);
	// TODO: Refine brute-force algorithms
	terrain::SAreaSector* GetAlternativeSector(terrain::SArea* sourceArea, const int sourceSIndex, terrain::SMobileType* destinationMT);
	terrain::SSector* GetAlternativeSector(terrain::SArea* destinationArea, const int sourceSIndex, terrain::SImmobileType* destinationIT); // can return 0
	const terrain::SSector& GetSector(int sIndex) const { return areaData->sector[sIndex]; }
public:
	const std::vector<terrain::SMobileType>& GetMobileTypes() const {
		return areaData->mobileType;
	}
	terrain::SMobileType* GetMobileType(CCircuitDef::Id unitDefId) const {
		return GetMobileTypeById(terrainData->udMobileType[unitDefId]);
	}
	terrain::SMobileType::Id GetMobileTypeId(CCircuitDef::Id unitDefId) const {
		return terrainData->udMobileType[unitDefId];
	}
	terrain::SMobileType* GetMobileTypeById(terrain::SMobileType::Id id) const {
		return (id < 0) ? nullptr : &areaData->mobileType[id];
	}
	const std::vector<terrain::SImmobileType>& GetImmobileTypes() const {
		return areaData->immobileType;
	}
	terrain::SImmobileType* GetImmobileType(CCircuitDef::Id unitDefId) const {
		return GetImmobileTypeById(terrainData->udImmobileType[unitDefId]);
	}
	terrain::SImmobileType::Id GetImmobileTypeId(CCircuitDef::Id unitDefId) const {
		return terrainData->udMobileType[unitDefId];
	}
	terrain::SImmobileType* GetImmobileTypeById(terrain::SImmobileType::Id id) const {
		return (id < 0) ? nullptr : &areaData->immobileType[id];
	}

	// position must be valid
	bool CanBeBuiltAt(CCircuitDef* cdef, const springai::AIFloat3& position, const float range);  // NOTE: returns false if the area was too small to be recorded
	bool CanBeBuiltAt(CCircuitDef* cdef, const springai::AIFloat3& position);
	bool CanBeBuiltAtSafe(CCircuitDef* cdef, const springai::AIFloat3& position);
	bool CanReachAt(CCircuitUnit* unit, const springai::AIFloat3& destination, const float range);
	bool CanReachAtSafe(CCircuitUnit* unit, const springai::AIFloat3& destination, const float range, const float threat = THREAT_MIN);
	bool CanReachAtSafe2(CCircuitUnit* unit, const springai::AIFloat3& destination, const float range);
	bool CanMobileReachAt(terrain::SArea* area, const springai::AIFloat3& destination, const float range);
	bool CanMobileReachAtSafe(terrain::SArea* area, const springai::AIFloat3& destination, const float range, const float threat = THREAT_MIN);

	float GetPercentLand() const { return areaData->percentLand; }
	float GetMinLandPercent() const { return minLandPercent; }
	bool IsWaterMap() const { return areaData->percentLand < minLandPercent; }
	bool IsWaterSector(const springai::AIFloat3& position) const {
		return areaData->sector[GetSectorIndex(position)].isWater;
	}

	const terrain::CArea* GetTAArea(const springai::AIFloat3& pos) const;
	const std::vector<terrain::CChokePoint*>& GetTAChokePoints() const { return terrainData->GetChokePoints(); }

	terrain::SAreaData* GetAreaData() const { return areaData; }
	void UpdateAreaUsers(int interval);
	void OnAreaUsersUpdated() { terrainData->OnAreaUsersUpdated(); }

	bool IsEnemyInArea(terrain::SArea* area) const {
		return enemyAreas.find(area) != enemyAreas.end();
	}

private:
	terrain::SAreaData* areaData;
	terrain::CTerrainData* terrainData;

	float minLandPercent;

	std::unordered_set<const terrain::SArea*> enemyAreas;

#ifdef DEBUG_VIS
private:
	int dbgTextureId;
	uint32_t sdlWindowId;
	float* dbgMap;
	bool isWidgetDrawing = false;
	void UpdateVis();
public:
	void ToggleVis();
	void ToggleWidgetDraw();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_TERRAIN_TERRAINMANAGER_H_
