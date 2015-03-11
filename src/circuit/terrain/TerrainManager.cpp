/*
 * TerrainManager.cpp
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#include "terrain/TerrainManager.h"
#include "terrain/BlockRectangle.h"
#include "terrain/BlockCircle.h"
#include "static/TerrainData.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"  // Only for UpdateAreaUsers
#include "resource/MetalManager.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "OOAICallback.h"
#include "WeaponDef.h"
#include "Pathing.h"
#include "MoveData.h"

#define STRUCT_BIT(bits)	static_cast<int>(SBlockingMap::StructMask::bits)

namespace circuit {

using namespace springai;

CTerrainManager::CTerrainManager(CCircuitAI* circuit, CTerrainData* terrainData) :
		circuit(circuit),
		terrainData(terrainData),
		cacheBuildFrame(0)
{
	Map* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	int mapHeight = map->GetHeight();
	terrainWidth = mapWidth * SQUARE_SIZE;
	terrainHeight = mapHeight * SQUARE_SIZE;

	UnitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();

	/*
	 * building masks
	 */
	UnitDef* def;
	WeaponDef* wpDef;
	int2 offset;
	int2 bsize;
	int2 ssize;
	int radius;
	int ignoreMask;
	def = circuit->GetUnitDefByName("factorycloak");
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize + int2(6, 4);
	// offset in South facing
	offset = int2(0, 4);
	ignoreMask = STRUCT_BIT(PYLON);
	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::FACTORY, ignoreMask);

	def = circuit->GetUnitDefByName("armsolar");
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::ENGY_LOW, ignoreMask);

	def = circuit->GetUnitDefByName("armwin");
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[def] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::ENGY_LOW, ignoreMask);

	def = circuit->GetUnitDefByName("armfus");
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON);
	blockInfos[def] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::ENGY_MID, ignoreMask);

	def = circuit->GetUnitDefByName("cafus");
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(ENGY_LOW) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[def] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::ENGY_HIGH, ignoreMask);

	def = circuit->GetUnitDefByName("armestor");
	const std::map<std::string, std::string>& customParams = def->GetCustomParams();
	auto search = customParams.find("pylonrange");
	float pylonRange = (search != customParams.end()) ? utils::string_to_float(search->second) : 500;
	radius = pylonRange / (SQUARE_SIZE * 1.3);
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ALL) & ~STRUCT_BIT(PYLON);
	blockInfos[def] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::PYLON, ignoreMask);

	def = circuit->GetUnitDefByName("armmstor");
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ALL) & ~STRUCT_BIT(FACTORY);
	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::MEX, ignoreMask);

	def = mexDef;  // cormex
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ALL) & ~STRUCT_BIT(FACTORY);
	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::MEX, ignoreMask);

	def = circuit->GetUnitDefByName("corrl");
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ENGY_LOW) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::DEF_LOW, ignoreMask);

	def = circuit->GetUnitDefByName("corllt");
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ENGY_LOW) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[def] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::DEF_LOW, ignoreMask);

	def = circuit->GetUnitDefByName("armnanotc");
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(PYLON);
	blockInfos[def] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::NANO, ignoreMask);

	blockingMap.columns = mapWidth / 2;  // build-step = 2 little green squares
	blockingMap.rows = mapHeight / 2;
	SBlockingMap::BlockCell cell = {0};
	blockingMap.grid.resize(blockingMap.columns * blockingMap.rows, cell);
	blockingMap.columnsLow = mapWidth / (GRID_RATIO_LOW * 2);
	blockingMap.rowsLow = mapHeight / (GRID_RATIO_LOW * 2);
	SBlockingMap::BlockCellLow cellLow = {0};
	blockingMap.gridLow.resize(blockingMap.columnsLow * blockingMap.rowsLow, cellLow);

	const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
	def = mexDef;
	int size = std::max(def->GetXSize(), def->GetZSize()) / 2;
	int& xsize = size, &zsize = size;
	int notIgnoreMask = STRUCT_BIT(FACTORY);
	for (auto& spot : spots) {
		const int x1 = int(spot.position.x / (SQUARE_SIZE << 1)) - (xsize >> 1), x2 = x1 + xsize;
		const int z1 = int(spot.position.z / (SQUARE_SIZE << 1)) - (zsize >> 1), z2 = z1 + zsize;
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.MarkBlocker(x, z, SBlockingMap::StructType::MEX, notIgnoreMask);
			}
		}
	}

	ignoreMask = STRUCT_BIT(PYLON);
	offset = int2(0, 0);
	CCircuitAI::UnitDefs& defs = circuit->GetUnitDefs();
	for (auto& kv : defs) {
		UnitDef* def = kv.second;
		if ((def->GetSpeed() == 0) && (blockInfos.find(def) == blockInfos.end())) {
			ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
			blockInfos[def] = new CBlockRectangle(offset, ssize, ssize, SBlockingMap::StructType::UNKNOWN, ignoreMask);
		}
	}

	if (!terrainData->IsInitialized()) {
		terrainData->Init(circuit);
	}
	areaData = terrainData->pAreaData.load();
}

CTerrainManager::~CTerrainManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& kv : blockInfos) {
		delete kv.second;
	}
}

int CTerrainManager::GetTerrainWidth()
{
	return terrainWidth;
}

int CTerrainManager::GetTerrainHeight()
{
	return terrainHeight;
}

void CTerrainManager::AddBlocker(UnitDef* unitDef, const AIFloat3& pos, int facing)
{
	Structure building = {-1, unitDef, pos, facing};
	MarkBlocker(building, true);
}

void CTerrainManager::RemoveBlocker(UnitDef* unitDef, const AIFloat3& pos, int facing)
{
	Structure building = {-1, unitDef, pos, facing};
	MarkBlocker(building, false);
}

AIFloat3 CTerrainManager::FindBuildSite(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing)
{
	TerrainPredicate predicate = [](const AIFloat3& p) {
		return true;
	};
	return FindBuildSite(unitDef, pos, searchRadius, facing, predicate);
}

AIFloat3 CTerrainManager::FindBuildSite(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing, TerrainPredicate& predicate)
{
	MarkAllyBuildings();

	auto search = blockInfos.find(unitDef);
	if (search != blockInfos.end()) {
		return FindBuildSiteByMask(unitDef, pos, searchRadius, facing, search->second, predicate);
	}

	if (searchRadius > SQUARE_SIZE * 2 * 100) {
		return FindBuildSiteLow(unitDef, pos, searchRadius, facing, predicate);
	}

	/*
	 * Default FindBuildSite
	 */
	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	auto isOpenSite = [this](const int2& s1, const int2& s2) {
		const int notIgnore = static_cast<int>(SBlockingMap::StructMask::ALL);
		for (int x = s1.x; x < s2.x; x++) {
			for (int z = s1.y; z < s2.y; z++) {
				if (blockingMap.IsBlocked(x, z, notIgnore)) {
					return false;
				}
			}
		}
		return true;
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const std::vector<SearchOffset>& ofs = GetSearchOffsetTable(endr);

	const int cornerX1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2);
	const int cornerZ1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2);

	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();
	CCircuitDef* cdef = circuit->GetCircuitDef(unitDef);

	for (int so = 0; so < endr * endr * 4; so++) {
		int2 s1(cornerX1 + ofs[so].dx, cornerZ1 + ofs[so].dy);
		int2 s2(    s1.x + xsize,          s1.y + zsize);
		if (!blockingMap.IsInBounds(s1, s2) || !isOpenSite(s1, s2)) {
			continue;
		}

		probePos.x = (s1.x + s2.x) * SQUARE_SIZE;
		probePos.z = (s1.y + s2.y) * SQUARE_SIZE;
		if (CanBeBuiltAt(cdef, probePos) && map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
			probePos.y = map->GetElevationAt(probePos.x, probePos.z);
			if (predicate(probePos)) {
				return probePos;
			}
		}
	}

	return -RgtVector;
}

void CTerrainManager::MarkAllyBuildings()
{
	if (!circuit->IsAllyAware()) {
		return;
	}

	int lastFrame = circuit->GetLastFrame();
	if (cacheBuildFrame + FRAMES_PER_SEC >= lastFrame) {
		return;
	}
	cacheBuildFrame = lastFrame;

	circuit->UpdateAllyUnits();
	const std::map<int, CCircuitUnit*>& allies = circuit->GetAllyUnits();
	UnitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();

	std::set<Structure, cmp> newUnits, oldUnits;
	for (auto& kv : allies) {
		CCircuitUnit* unit = kv.second;
		Unit* u = unit->GetUnit();
		if (u->GetMaxSpeed() <= 0) {
			int unitId = kv.first;
			Structure building;
			building.unitId = unitId;
			decltype(markedAllies)::iterator search = markedAllies.find(building);
			if (search == markedAllies.end()) {
				UnitDef* def = u->GetDef();
				building.def = circuit->GetUnitDefById(def->GetUnitDefId());
				building.pos = u->GetPos();
				building.facing = u->GetBuildingFacing();
				delete def;
				newUnits.insert(building);
				if (building.def == mexDef) {  // update metalInfo's open state
					circuit->GetMetalManager()->SetOpenSpot(building.pos, false);
				} else {
					MarkBlocker(building, true);
				}
			} else {
				oldUnits.insert(*search);
			}
		}
	}
	std::set<Structure, cmp> deadUnits;
	std::set_difference(markedAllies.begin(), markedAllies.end(),
						oldUnits.begin(), oldUnits.end(),
						std::inserter(deadUnits, deadUnits.begin()), cmp());
	for (auto& building : deadUnits) {
		if (building.def == mexDef) {  // update metalInfo's open state
			circuit->GetMetalManager()->SetOpenSpot(building.pos, true);
		} else {
			MarkBlocker(building, false);
		}
	}
	markedAllies.clear();
	std::set_union(oldUnits.begin(), oldUnits.end(),
				   newUnits.begin(), newUnits.end(),
				   std::inserter(markedAllies, markedAllies.begin()), cmp());
}

const CTerrainManager::SearchOffsets& CTerrainManager::GetSearchOffsetTable(int radius)
{
	static std::vector<SearchOffset> searchOffsets;
	unsigned int size = radius * radius * 4;
	if (size > searchOffsets.size()) {
		searchOffsets.resize(size);

		for (int y = 0; y < radius * 2; y++) {
			for (int x = 0; x < radius * 2; x++) {
				SearchOffset& i = searchOffsets[y * radius * 2 + x];

				i.dx = x - radius;
				i.dy = y - radius;
				i.qdist = i.dx * i.dx + i.dy * i.dy;
			}
		}

		auto searchOffsetComparator = [](const SearchOffset& a, const SearchOffset& b) {
			return a.qdist < b.qdist;
		};
		std::sort(searchOffsets.begin(), searchOffsets.end(), searchOffsetComparator);
	}

	return searchOffsets;
}

const CTerrainManager::SearchOffsetsLow& CTerrainManager::GetSearchOffsetTableLow(int radius)
{
	static SearchOffsetsLow searchOffsetsLow;
	int radiusLow = radius / GRID_RATIO_LOW;
	unsigned int sizeLow = radiusLow * radiusLow * 4;
	if (sizeLow > searchOffsetsLow.size()) {
		searchOffsetsLow.resize(sizeLow);

		SearchOffsets searchOffsets;
		searchOffsets.resize(radius * radius * 4);
		for (int y = 0; y < radius * 2; y++) {
			for (int x = 0; x < radius * 2; x++) {
				SearchOffset& i = searchOffsets[y * radius * 2 + x];
				i.dx = x - radius;
				i.dy = y - radius;
				i.qdist = i.dx * i.dx + i.dy * i.dy;
			}
		}

		auto searchOffsetComparator = [](const SearchOffset& a, const SearchOffset& b) {
			return a.qdist < b.qdist;
		};
		for (int yl = 0; yl < radiusLow * 2; yl++) {
			for (int xl = 0; xl < radiusLow * 2; xl++) {
				SearchOffsetLow& il = searchOffsetsLow[yl * radiusLow * 2 + xl];
				il.dx = xl - radiusLow;
				il.dy = yl - radiusLow;
				il.qdist = il.dx * il.dx + il.dy * il.dy;

				il.ofs.reserve(GRID_RATIO_LOW * GRID_RATIO_LOW);
				const int xi = xl * GRID_RATIO_LOW;
				for (int y = yl * GRID_RATIO_LOW; y < (yl + 1) * GRID_RATIO_LOW; y++) {
					for (int x = xi; x < xi + GRID_RATIO_LOW; x++) {
						il.ofs.push_back(searchOffsets[y * radius * 2 + x]);
					}
				}

				std::sort(il.ofs.begin(), il.ofs.end(), searchOffsetComparator);
			}
		}

		auto searchOffsetLowComparator = [](const SearchOffsetLow& a, const SearchOffsetLow& b) {
			return a.qdist < b.qdist;
		};
		std::sort(searchOffsetsLow.begin(), searchOffsetsLow.end(), searchOffsetLowComparator);
	}

	return searchOffsetsLow;
}

AIFloat3 CTerrainManager::FindBuildSiteLow(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing, TerrainPredicate& predicate)
{
	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	auto isOpenSite = [this](const int2& s1, const int2& s2) {
		const int notIgnore = static_cast<int>(SBlockingMap::StructMask::ALL);
		for (int x = s1.x; x < s2.x; x++) {
			for (int z = s1.y; z < s2.y; z++) {
				if (blockingMap.IsBlocked(x, z, notIgnore)) {
					return false;
				}
			}
		}
		return true;
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const SearchOffsetsLow& ofsLow = GetSearchOffsetTableLow(endr);
	const int endrLow = endr / GRID_RATIO_LOW;

	const int centerX = int(pos.x / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));
	const int centerZ = int(pos.z / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));

	const int cornerX1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2);
	const int cornerZ1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2);

	const int notIgnore = static_cast<int>(SBlockingMap::StructMask::ALL);

	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();
	CCircuitDef* cdef = circuit->GetCircuitDef(unitDef);

	for (int soLow = 0; soLow < endrLow * endrLow * 4; soLow++) {
		int xlow = centerX + ofsLow[soLow].dx;
		int zlow = centerZ + ofsLow[soLow].dy;
		if (!blockingMap.IsInBoundsLow(xlow, zlow) || blockingMap.IsBlockedLow(xlow, zlow, notIgnore)) {
			continue;
		}

		const SearchOffsets& ofs = ofsLow[soLow].ofs;
		for (int so = 0; so < GRID_RATIO_LOW * GRID_RATIO_LOW; so++) {
			int2 s1(cornerX1 + ofs[so].dx, cornerZ1 + ofs[so].dy);
			int2 s2(    s1.x + xsize,          s1.y + zsize);
			if (!blockingMap.IsInBounds(s1, s2) || !isOpenSite(s1, s2)) {
				continue;
			}

			probePos.x = (s1.x + s2.x) * SQUARE_SIZE;
			probePos.z = (s1.y + s2.y) * SQUARE_SIZE;
			if (CanBeBuiltAt(cdef, probePos) && map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
				probePos.y = map->GetElevationAt(probePos.x, probePos.z);
				if (predicate(probePos)) {
					return probePos;
				}
			}
		}
	}

	return -RgtVector;
}

AIFloat3 CTerrainManager::FindBuildSiteByMask(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask, TerrainPredicate& predicate)
{
	int xmsize = mask->GetXSize();
	int zmsize = mask->GetZSize();
	if ((searchRadius > SQUARE_SIZE * 2 * 100) || (xmsize * zmsize > GRID_RATIO_LOW * GRID_RATIO_LOW * 9)) {
		return FindBuildSiteByMaskLow(unitDef, pos, searchRadius, facing, mask, predicate);
	}

	int xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
			break;
		}
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
			break;
		}
	}

#define DECLARE_TEST(testName, facingType)																	\
	auto testName = [this, mask, notIgnore, structMask](const int2& m1, const int2& m2, const int2& om) {	\
		for (int x = m1.x, xm = om.x; x < m2.x; x++, xm++) {												\
			for (int z = m1.y, zm = om.y; z < m2.y; z++, zm++) {											\
				switch (mask->facingType(xm, zm)) {															\
					case IBlockMask::BlockType::BLOCKED: {													\
						if (blockingMap.IsStruct(x, z, structMask)) {										\
							return false;																	\
						}																					\
						break;																				\
					}																						\
					case IBlockMask::BlockType::STRUCT: {													\
						if (blockingMap.IsBlocked(x, z, notIgnore)) {										\
							return false;																	\
						}																					\
						break;																				\
					}																						\
				}																							\
			}																								\
		}																									\
		return true;																						\
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const SearchOffsets& ofs = GetSearchOffsetTable(endr);

	int2 structCorner;
	structCorner.x = int(pos.x / (SQUARE_SIZE * 2)) - (xssize / 2);
	structCorner.y = int(pos.z / (SQUARE_SIZE * 2)) - (zssize / 2);

	const int2& offset = mask->GetStructOffset(facing);
	int2 maskCorner = structCorner - offset;

	const int notIgnore = ~mask->GetIgnoreMask();
	SBlockingMap::StructMask structMask = SBlockingMap::GetStructMask(mask->GetStructType());

	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();
	CCircuitDef* cdef = circuit->GetCircuitDef(unitDef);

#define DO_TEST(testName)																			\
	for (int so = 0; so < endr * endr * 4; so++) {													\
		int2 s1(structCorner.x + ofs[so].dx, structCorner.y + ofs[so].dy);							\
		int2 s2(          s1.x + xssize,               s1.y + zssize);								\
		if (!blockingMap.IsInBounds(s1, s2)) {														\
			continue;																				\
		}																							\
																									\
		int2 m1(maskCorner.x + ofs[so].dx, maskCorner.y + ofs[so].dy);								\
		int2 m2(        m1.x + xmsize,             m1.y + zmsize);									\
		int2 om = m1;																				\
		blockingMap.Bound(m1, m2);																	\
		om = m1 - om;																				\
		if (!testName(m1, m2, om)) {																\
			continue;																				\
		}																							\
																									\
		probePos.x = (s1.x + s2.x) * SQUARE_SIZE;													\
		probePos.z = (s1.y + s2.y) * SQUARE_SIZE;													\
		if (CanBeBuiltAt(cdef, probePos) && map->IsPossibleToBuildAt(unitDef, probePos, facing)) {	\
			probePos.y = map->GetElevationAt(probePos.x, probePos.z);								\
			if (predicate(probePos)) {																\
				return probePos;																	\
			}																						\
		}																							\
	}

	switch (facing) {
		default:
		case UNIT_FACING_SOUTH: {
			DECLARE_TEST(isOpenSouth, GetTypeSouth);
			DO_TEST(isOpenSouth);
			break;
		}
		case UNIT_FACING_EAST: {
			DECLARE_TEST(isOpenEast, GetTypeEast);
			DO_TEST(isOpenEast);
			break;
		}
		case UNIT_FACING_NORTH: {
			DECLARE_TEST(isOpenNorth, GetTypeNorth);
			DO_TEST(isOpenNorth);
			break;
		}
		case UNIT_FACING_WEST: {
			DECLARE_TEST(isOpenWest, GetTypeWest);
			DO_TEST(isOpenWest);
			break;
		}
	}

	return -RgtVector;
}

AIFloat3 CTerrainManager::FindBuildSiteByMaskLow(UnitDef* unitDef, const AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask, TerrainPredicate& predicate)
{
	int xmsize, zmsize, xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xmsize = mask->GetXSize();
			zmsize = mask->GetZSize();
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
			break;
		}
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
			break;
		}
	}

#define DECLARE_TEST_LOW(testName, facingType)																\
	auto testName = [this, mask, notIgnore, structMask](const int2& m1, const int2& m2, const int2& om) {	\
		for (int x = m1.x, xm = om.x; x < m2.x; x++, xm++) {												\
			for (int z = m1.y, zm = om.y; z < m2.y; z++, zm++) {											\
				switch (mask->facingType(xm, zm)) {															\
					case IBlockMask::BlockType::BLOCKED: {													\
						if (blockingMap.IsStruct(x, z, structMask)) {										\
							return false;																	\
						}																					\
						break;																				\
					}																						\
					case IBlockMask::BlockType::STRUCT: {													\
						if (blockingMap.IsBlocked(x, z, notIgnore)) {										\
							return false;																	\
						}																					\
						break;																				\
					}																						\
				}																							\
			}																								\
		}																									\
		return true;																						\
	};

	const int endr = (int)(searchRadius / (SQUARE_SIZE * 2));
	const SearchOffsetsLow& ofsLow = GetSearchOffsetTableLow(endr);
	const int endrLow = endr / GRID_RATIO_LOW;

	int2 structCorner;
	structCorner.x = int(pos.x / (SQUARE_SIZE * 2)) - (xssize / 2);
	structCorner.y = int(pos.z / (SQUARE_SIZE * 2)) - (zssize / 2);

	const int2& offset = mask->GetStructOffset(facing);
	int2 maskCorner = structCorner - offset;

	int2 structCenter;
	structCenter.x = int(pos.x / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));
	structCenter.y = int(pos.z / (SQUARE_SIZE * 2 * GRID_RATIO_LOW));

	const int notIgnore = ~mask->GetIgnoreMask();
	SBlockingMap::StructMask structMask = SBlockingMap::GetStructMask(mask->GetStructType());

	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();
	CCircuitDef* cdef = circuit->GetCircuitDef(unitDef);

#define DO_TEST_LOW(testName)																					\
	for (int soLow = 0; soLow < endrLow * endrLow * 4; soLow++) {												\
		int2 low(structCenter.x + ofsLow[soLow].dx, structCenter.y + ofsLow[soLow].dy);							\
		if (!blockingMap.IsInBoundsLow(low.x, low.y) || blockingMap.IsBlockedLow(low.x, low.y, notIgnore)) {	\
			continue;																							\
		}																										\
																												\
		const SearchOffsets& ofs = ofsLow[soLow].ofs;															\
		for (int so = 0; so < GRID_RATIO_LOW * GRID_RATIO_LOW; so++) {											\
			int2 s1(structCorner.x + ofs[so].dx, structCorner.y + ofs[so].dy);									\
			int2 s2(          s1.x + xssize,               s1.y + zssize);										\
			if (!blockingMap.IsInBounds(s1, s2)) {																\
				continue;																						\
			}																									\
																												\
			int2 m1(maskCorner.x + ofs[so].dx, maskCorner.y + ofs[so].dy);										\
			int2 m2(        m1.x + xmsize,             m1.y + zmsize);											\
			int2 om = m1;																						\
			blockingMap.Bound(m1, m2);																			\
			om = m1 - om;																						\
			if (!testName(m1, m2, om)) {																		\
				continue;																						\
			}																									\
																												\
			probePos.x = (s1.x + s2.x) * SQUARE_SIZE;															\
			probePos.z = (s1.y + s2.y) * SQUARE_SIZE;															\
			if (CanBeBuiltAt(cdef, probePos) && map->IsPossibleToBuildAt(unitDef, probePos, facing)) {			\
				probePos.y = map->GetElevationAt(probePos.x, probePos.z);										\
				if (predicate(probePos)) {																		\
					return probePos;																			\
				}																								\
			}																									\
		}																										\
	}

	switch (facing) {
		default:
		case UNIT_FACING_SOUTH: {
			DECLARE_TEST_LOW(isOpenSouth, GetTypeSouth);
			DO_TEST_LOW(isOpenSouth);
			break;
		}
		case UNIT_FACING_EAST: {
			DECLARE_TEST_LOW(isOpenEast, GetTypeEast);
			DO_TEST_LOW(isOpenEast);
			break;
		}
		case UNIT_FACING_NORTH: {
			DECLARE_TEST_LOW(isOpenNorth, GetTypeNorth);
			DO_TEST_LOW(isOpenNorth);
			break;
		}
		case UNIT_FACING_WEST: {
			DECLARE_TEST_LOW(isOpenWest, GetTypeWest);
			DO_TEST_LOW(isOpenWest);
			break;
		}
	}

	return -RgtVector;
}

void CTerrainManager::MarkBlockerByMask(const Structure& building, bool block, IBlockMask* mask)
{
	UnitDef* unitDef = building.def;
	int facing = building.facing;
	const AIFloat3& pos = building.pos;

	int xmsize, zmsize, xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xmsize = mask->GetXSize();
			zmsize = mask->GetZSize();
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
			break;
		}
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
			break;
		}
	}

#define DECLARE_MARKER(typeName, blockerOp, structOp)					\
	for (int x = m1.x, xm = om.x; x < m2.x; x++, xm++) {				\
		for (int z = m1.y, zm = om.y; z < m2.y; z++, zm++) {			\
			switch (mask->typeName(xm, zm)) {							\
				case IBlockMask::BlockType::BLOCKED: {					\
					blockingMap.blockerOp(x, z, structType);			\
					break;												\
				}														\
				case IBlockMask::BlockType::STRUCT: {					\
					blockingMap.structOp(x, z, structType, notIgnore);	\
					break;												\
				}														\
			}															\
		}																\
	}

	int2 corner;
	corner.x = int(pos.x / (SQUARE_SIZE * 2)) - (xssize / 2);
	corner.y = int(pos.z / (SQUARE_SIZE * 2)) - (zssize / 2);

	int2 m1 = corner - mask->GetStructOffset(facing);	// top-left mask corner
	int2 m2(m1.x + xmsize, m1.y + zmsize);				// bottom-right mask corner
	int2 om = m1;										// remember original mask corner
	blockingMap.Bound(m1, m2);							// corners bounded by map
	om = m1 - om;										// shift original mask corner

	const int notIgnore = ~mask->GetIgnoreMask();
	SBlockingMap::StructType structType = mask->GetStructType();

	Map* map = circuit->GetMap();

#define DO_MARK(facingType)												\
	if (block) {														\
		DECLARE_MARKER(facingType, AddBlocker, AddStruct);				\
	} else {															\
		DECLARE_MARKER(facingType, RemoveBlocker, RemoveStruct);		\
	}

	switch (facing) {
		default:
		case UNIT_FACING_SOUTH: {
			DO_MARK(GetTypeSouth);
			break;
		}
		case UNIT_FACING_EAST: {
			DO_MARK(GetTypeEast);
			break;
		}
		case UNIT_FACING_NORTH: {
			DO_MARK(GetTypeNorth);
			break;
		}
		case UNIT_FACING_WEST: {
			DO_MARK(GetTypeWest);
			break;
		}
	}
}

void CTerrainManager::MarkBlocker(const Structure& building, bool block)
{
	UnitDef* unitDef = building.def;
	auto search = blockInfos.find(unitDef);
	if (search != blockInfos.end()) {
		MarkBlockerByMask(building, block, search->second);
		return;
	}

	/*
	 * Default marker
	 */
	int facing = building.facing;
	const AIFloat3& pos = building.pos;

	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	const int x1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2), x2 = x1 + xsize;
	const int z1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2), z2 = z1 + zsize;

	const SBlockingMap::StructType structType = SBlockingMap::StructType::UNKNOWN;
	const int notIgnore = static_cast<int>(SBlockingMap::StructMask::ALL);

	if (block) {
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.AddStruct(x, z, structType, notIgnore);
			}
		}
	} else {
		// NOTE: This can mess up things if unit is inside factory :/
		// SOLUTION: Do not mark movable units
		for (int z = z1; z < z2; z++) {
			for (int x = x1; x < x2; x++) {
				blockingMap.RemoveStruct(x, z, structType, notIgnore);
			}
		}
	}
}

//void CTerrainManager::CorrectPosition(AIFloat3& position)
//{
//	if (position.x < 1) {
//		position.x = 1;
//	} else if (position.x > terrainWidth - 2) {
//		position.x = terrainWidth - 2;
//	}
//	if (position.z < 1) {
//		position.z = 1;
//	} else if (position.z > terrainHeight - 2) {
//		position.z = terrainHeight - 2;
//	}
//	position.y = circuit->GetMap()->GetElevationAt(position.x, position.z);
//}

STerrainMapArea* CTerrainManager::GetCurrentMapArea(CCircuitDef* cdef, const AIFloat3& position)
{
	STerrainMapMobileType* mobileType = GetMobileTypeById(cdef->GetMobileTypeId());
	if (mobileType == nullptr) {  // flying units & buildings
		return nullptr;
	}

	// other mobile units & their factories
	int iS = GetSectorIndex(position);
//	if (!terrainData->IsSectorValid(iS)) {
//		CorrectPosition(position);
//		iS = GetSectorIndex(position);
//	}
	STerrainMapArea* area = mobileType->sector[iS].area;
	if (area == nullptr) {  // unit outside of valid area
		area = GetAlternativeSector(area, iS, mobileType)->area;
	}
	return area;
}

int CTerrainManager::GetSectorIndex(const AIFloat3& position)
{
	return terrainData->GetSectorIndex(position);
}

bool CTerrainManager::CanMoveToPos(STerrainMapArea* area, const AIFloat3& destination)
{
	return terrainData->CanMoveToPos(area, destination);
}

STerrainMapAreaSector* CTerrainManager::GetClosestSector(STerrainMapArea* sourceArea, const int& destinationSIndex)
{
	return terrainData->GetClosestSector(sourceArea, destinationSIndex);
}

STerrainMapSector* CTerrainManager::GetClosestSector(STerrainMapImmobileType* sourceIT, const int& destinationSIndex)
{
	return terrainData->GetClosestSector(sourceIT, destinationSIndex);
}

STerrainMapAreaSector* CTerrainManager::GetAlternativeSector(STerrainMapArea* sourceArea, const int& sourceSIndex, STerrainMapMobileType* destinationMT)
{
	return terrainData->GetAlternativeSector(sourceArea, sourceSIndex, destinationMT);
}

STerrainMapSector* CTerrainManager::GetAlternativeSector(STerrainMapArea* destinationArea, const int& sourceSIndex, STerrainMapImmobileType* destinationIT)
{
	return terrainData->GetAlternativeSector(destinationArea, sourceSIndex, destinationIT);
}

const STerrainMapSector& CTerrainManager::GetSector(int sIndex) const
{
	return areaData->sector[sIndex];
}

int CTerrainManager::GetConvertStoP() const
{
	return terrainData->convertStoP;
}

STerrainMapMobileType* CTerrainManager::GetMobileType(int unitDefId) const
{
	return GetMobileTypeById(terrainData->udMobileType[unitDefId]);
}

int CTerrainManager::GetMobileTypeId(int unitDefId) const
{
	return terrainData->udMobileType[unitDefId];
}

STerrainMapMobileType* CTerrainManager::GetMobileTypeById(int id) const
{
	return (id < 0) ? nullptr : &areaData->mobileType[id];
}

STerrainMapImmobileType* CTerrainManager::GetImmobileType(int unitDefId) const
{
	return GetImmobileTypeById(terrainData->udImmobileType[unitDefId]);
}

int CTerrainManager::GetImmobileTypeId(int unitDefId) const
{
	return terrainData->udMobileType[unitDefId];
}

STerrainMapImmobileType* CTerrainManager::GetImmobileTypeById(int id) const
{
	return &areaData->immobileType[id];
}

bool CTerrainManager::CanBeBuiltAt(CCircuitDef* cdef, const AIFloat3& position, const float& range)
{
	int iS = GetSectorIndex(position);
	STerrainMapSector* sector;
	STerrainMapMobileType* mobileType = GetMobileTypeById(cdef->GetMobileTypeId());
	STerrainMapImmobileType* immobileType = GetImmobileTypeById(cdef->GetImmobileTypeId());
	if (mobileType != nullptr) {  // a factory or mobile unit
		STerrainMapAreaSector* AS = GetAlternativeSector(nullptr, iS, mobileType);
		if (immobileType != nullptr) {  // a factory
			sector = GetAlternativeSector(AS->area, iS, immobileType);
			if (sector == 0) {
				return false;
			}
		} else {
			sector = AS->S;
		}
	} else if (immobileType != nullptr) {  // buildings
		sector = GetClosestSector(immobileType, iS);
	} else {
		return true;  // flying units
	}

	if (sector == &GetSector(iS)) {  // the current sector is the best sector
		return true;
	}
	return sector->position.distance2D(GetSector(iS).position) < range;
}

bool CTerrainManager::CanBuildAt(CCircuitUnit* unit, const AIFloat3& destination)
{
	// FIXME: so far we know only mobile builders
//	if (unit->GetCircuitDef()->GetImmobileType() != nullptr) {  // A hub or factory
//		return unit->GetUnit()->GetPos().distance2D(destination) < unit->GetDef()->GetBuildDistance();
//	}
	STerrainMapArea* area = unit->GetArea();
	if (area == nullptr) {  // A flying unit
		return true;
	}
	int iS = GetSectorIndex(destination);
	if (area->sector.find(iS) != area->sector.end()) {
		return true;
	}
	if (GetClosestSector(area, iS)->S->position.distance2D(destination) < unit->GetDef()->GetBuildDistance() - GetConvertStoP()) {
		return true;
	}
	return false;
}

void CTerrainManager::UpdateAreaUsers()
{
	areaData = terrainData->GetNextAreaData();
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		unit->SetArea(GetCurrentMapArea(unit->GetCircuitDef(), unit->GetUnit()->GetPos()));
	}
	// TODO: Use boost signals to invoke UpdateAreaUsers event?
	circuit->GetBuilderManager()->UpdateAreaUsers();

	terrainData->DidUpdateAreaUsers();
}

void CTerrainManager::ClusterizeTerrain()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	terrainData->SetClusterizing(true);

	// step 1: Create waypoints
	Pathing* pathing = circuit->GetPathing();
	Map* map = circuit->GetMap();
	const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
	std::vector<AIFloat3> wayPoints;
	for (auto& spot : spots) {
		AIFloat3 start = spot.position;
		for (auto& s : spots) {
			if (spot.position == s.position) {
				continue;
			}
			AIFloat3 end = s.position;
			int pathId = pathing->InitPath(start, end, 4, .0f);
			AIFloat3 lastPoint, point(start);
			int j = 0;
			do {
				lastPoint = point;
				point = pathing->GetNextWaypoint(pathId);
				if (point.x <= 0 || point.z <= 0) {
					break;
				}
				if (j++ % 32 == 0) {
					wayPoints.push_back(point);
				}
			} while ((lastPoint != point) && (point.x > 0 && point.z > 0));
			pathing->FreePath(pathId);
		}
	}

	// step 2: Create path traversability map
	// @see path traversability map rts/
	int widthX = circuit->GetMap()->GetWidth();
	int heightZ = circuit->GetMap()->GetHeight();
	int widthSX = widthX / 2;
	MoveData* moveDef = circuit->GetUnitDefByName("armcom1")->GetMoveData();
	float maxSlope = moveDef->GetMaxSlope();
	float depth = moveDef->GetDepth();
	float slopeMod = moveDef->GetSlopeMod();
	const std::vector<float>& heightMap = circuit->GetMap()->GetHeightMap();
	const std::vector<float>& slopeMap = circuit->GetMap()->GetSlopeMap();

	std::vector<float> traversMap(widthX * heightZ);

	auto posSpeedMod = [](float maxSlope, float depth, float slopeMod, float depthMod, float height, float slope) {
		float speedMod = 0.0f;

		// slope too steep or square too deep?
		if (slope > maxSlope)
			return speedMod;
		if (-height > depth)
			return speedMod;

		// slope-mod
		speedMod = 1.0f / (1.0f + slope * slopeMod);
		// FIXME: Read waterDamageCost from mapInfo
//			speedMod *= (height < 0.0f) ? waterDamageCost : 1.0f;
		speedMod *= depthMod;

		return speedMod;
	};

	for (int hz = 0; hz < heightZ; ++hz) {
		for (int hx = 0; hx < widthX; ++hx) {
			const int sx = hx / 2;  // hx >> 1;
			const int sz = hz / 2;  // hz >> 1;
//				const bool losSqr = losHandler->InLos(sqx, sqy, gu->myAllyTeam);

			float scale = 1.0f;

			// TODO: First implement blocking map
//				if (los || losSqr) {
//					if (CMoveMath::IsBlocked(*md, sqx,     sqy    , NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
//					if (CMoveMath::IsBlocked(*md, sqx + 1, sqy    , NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
//					if (CMoveMath::IsBlocked(*md, sqx,     sqy + 1, NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
//					if (CMoveMath::IsBlocked(*md, sqx + 1, sqy + 1, NULL) & CMoveMath::BLOCK_STRUCTURE) { scale -= 0.25f; }
//				}

			int idx = hz * widthX + hx;
			float height = heightMap[idx];
			float slope = slopeMap[sz * widthSX + sx];
			float depthMod = moveDef->GetDepthMod(height);
			traversMap[idx] = posSpeedMod(maxSlope, depth, slopeMod, depthMod, height, slope);
			// FIXME: blocking map first
//				const SColor& smc = GetSpeedModColor(sm * scale);
		}
	}
	delete moveDef;

	// step 3: Filter key waypoints
	printf("points size: %i\n", wayPoints.size());
	auto iter = wayPoints.begin();
	while (iter != wayPoints.end()) {
		bool isKey = false;
		if ((iter->z / SQUARE_SIZE - 10 >= 0 && iter->z / SQUARE_SIZE + 10 < heightZ) && (iter->x / SQUARE_SIZE - 10 >= 0 && iter->x / SQUARE_SIZE + 10 < widthX)) {
			int idx = (int)(iter->z / SQUARE_SIZE) * widthX + (int)(iter->x / SQUARE_SIZE);
			if (traversMap[idx] > 0.8) {
				for (int hz = -10; hz <= 10; ++hz) {
					for (int hx = -10; hx <= 10; ++hx) {
						idx = (int)(iter->z / SQUARE_SIZE + hz) * widthX + iter->x / SQUARE_SIZE + hx;
						if (traversMap[idx] < 0.8) {
							isKey = true;
							break;
						}
					}
					if (isKey) {
						break;
					}
				}
			}
		}
		if (!isKey) {
			iter = wayPoints.erase(iter);
		} else {
			++iter;
		}
	}

	// step 4: Clusterize key waypoints
	float maxDistance = circuit->GetUnitDefByName("cordoom")->GetMaxWeaponRange() * 2;
	maxDistance *= maxDistance;
	circuit->GetScheduler()->RunParallelTask(std::make_shared<CGameTask>(&CTerrainData::Clusterize, terrainData, wayPoints, maxDistance, circuit));
}

const std::vector<springai::AIFloat3>& CTerrainManager::GetDefencePoints() const
{
	return terrainData->GetDefencePoints();
}

const std::vector<springai::AIFloat3>& CTerrainManager::GetDefencePerimeter() const
{
	return terrainData->GetDefencePerimeter();
}

} // namespace circuit
