/*
 * TerrainManager.cpp
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#include "terrain/TerrainManager.h"
#include "terrain/BlockRectangle.h"
#include "terrain/BlockCircle.h"
#include "terrain/ThreatMap.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"  // Only for UpdateAreaUsers
#include "resource/MetalManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"

#include "Map.h"
#include "OOAICallback.h"
#include "WeaponDef.h"
#include "Pathing.h"
#include "MoveData.h"

namespace circuit {

using namespace springai;

CTerrainManager::CTerrainManager(CCircuitAI* circuit, CTerrainData* terrainData)
		: circuit(circuit)
		, terrainData(terrainData)
#ifdef DEBUG_VIS
		, dbgTextureId(-1)
		, sdlWindowId(-1)
		, dbgMap(nullptr)
#endif
{
	ResetBuildFrame();

	Map* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	int mapHeight = map->GetHeight();
	terrainWidth = mapWidth * SQUARE_SIZE;
	terrainHeight = mapHeight * SQUARE_SIZE;

	CCircuitDef* mexDef = circuit->GetCircuitDef("cormex");

	/*
	 * building masks
	 */
	CCircuitDef* cdef;
	UnitDef* def;
	WeaponDef* wpDef;
	int2 offset;
	int2 bsize;
	int2 ssize;
	int radius;
	int ignoreMask;

	// offset in South facing
	offset = int2(0, 4);
	ignoreMask = STRUCT_BIT(NONE);
	const char* factories[] = {"factorycloak", "factoryamph", "factoryhover", "factoryjump", "factoryshield", "factoryspider", "factorytank", "factoryveh"};
	for (const char* fac : factories) {
		cdef = circuit->GetCircuitDef(fac);
		def = cdef->GetUnitDef();
		ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
		bsize = ssize + int2(8, 8);
		blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::FACTORY, ignoreMask);
	}

	cdef = circuit->GetCircuitDef("striderhub");
	def = cdef->GetUnitDef();
	radius = 250 /*cdef->GetBuildDistance()*/ / (SQUARE_SIZE * 2);
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(NONE);
	blockInfos[cdef->GetId()] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::SPECIAL, ignoreMask);

	cdef = circuit->GetCircuitDef("armsolar");
	def = cdef->GetUnitDef();
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::ENGY_LOW, ignoreMask);

	cdef = circuit->GetCircuitDef("armwin");
	def = cdef->GetUnitDef();
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[cdef->GetId()] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::ENGY_LOW, ignoreMask);

	cdef = circuit->GetCircuitDef("armfus");
	def = cdef->GetUnitDef();
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(DEF_LOW);
	blockInfos[cdef->GetId()] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::ENGY_MID, ignoreMask);

	cdef = circuit->GetCircuitDef("cafus");
	def = cdef->GetUnitDef();
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	radius -= radius / 6 * (std::min(circuit->GetAllyTeam()->GetSize(), 4) - 1);  // [radius ~ 1 player ; radius/2 ~ 4+ players]
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(ENGY_LOW) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[cdef->GetId()] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::ENGY_HIGH, ignoreMask);

	cdef = circuit->GetCircuitDef("armestor");
	def = cdef->GetUnitDef();
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ALL) & ~(STRUCT_BIT(FACTORY) | STRUCT_BIT(PYLON));
	blockInfos[cdef->GetId()] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::PYLON, ignoreMask);

	cdef = circuit->GetCircuitDef("armmstor");
	def = cdef->GetUnitDef();
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ALL) & ~STRUCT_BIT(FACTORY);
	blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::MEX, ignoreMask);

	cdef = mexDef;
	def = cdef->GetUnitDef();  // cormex
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ALL) & ~(STRUCT_BIT(FACTORY) | STRUCT_BIT(PYLON));
	blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::MEX, ignoreMask);

	cdef = circuit->GetCircuitDef("corrl");
	def = cdef->GetUnitDef();
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize + int2(4, 4);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ENGY_LOW) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::DEF_LOW, ignoreMask);

	cdef = circuit->GetCircuitDef("corllt");
	def = cdef->GetUnitDef();
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	bsize = ssize + int2(4, 4);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(ENGY_LOW) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(ENGY_HIGH) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(NANO);
	blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::DEF_LOW, ignoreMask);

	cdef = circuit->GetCircuitDef("armnanotc");
	def = cdef->GetUnitDef();
//	wpDef = def->GetDeathExplosion();
//	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
//	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
//	bsize = int2(radius * 2 - (ssize.x % 2), radius * 2 - (ssize.y % 2));
	bsize = ssize;
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(ENGY_HIGH);
	blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::NANO, ignoreMask);

	cdef = circuit->GetCircuitDef("raveparty");
	def = cdef->GetUnitDef();
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(ENGY_HIGH);
	blockInfos[cdef->GetId()] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::SPECIAL, ignoreMask);

	cdef = circuit->GetCircuitDef("armamd");
	def = cdef->GetUnitDef();
	wpDef = def->GetDeathExplosion();
	radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
	delete wpDef;
	ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(MEX) |
				 STRUCT_BIT(DEF_LOW) |
				 STRUCT_BIT(ENGY_MID) |
				 STRUCT_BIT(PYLON) |
				 STRUCT_BIT(ENGY_HIGH);
	blockInfos[cdef->GetId()] = new CBlockCircle(offset, radius, ssize, SBlockingMap::StructType::SPECIAL, ignoreMask);

	const char* striders[] = {"armcomdgun", "scorpion", "dante", "armraven", "funnelweb", "armbanth", "armorco"};
	for (const char* strider : striders) {
		cdef = circuit->GetCircuitDef(strider);
		def = cdef->GetUnitDef();
		ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
		bsize = ssize + int2(4, 4);
		offset = int2(0, 0);
		ignoreMask = STRUCT_BIT(ALL);
		blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::SPECIAL, ignoreMask);
	}

	blockingMap.columns = mapWidth / 2;  // build-step = 2 little green squares
	blockingMap.rows = mapHeight / 2;
	SBlockingMap::SBlockCell cell = {0};
	blockingMap.grid.resize(blockingMap.columns * blockingMap.rows, cell);
	blockingMap.columnsLow = mapWidth / (GRID_RATIO_LOW * 2);
	blockingMap.rowsLow = mapHeight / (GRID_RATIO_LOW * 2);
	SBlockingMap::SBlockCellLow cellLow = {0};
	blockingMap.gridLow.resize(blockingMap.columnsLow * blockingMap.rowsLow, cellLow);

	const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
	def = mexDef->GetUnitDef();
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

	offset = int2(0, 0);
	ignoreMask = STRUCT_BIT(PYLON) |
				 STRUCT_BIT(ENGY_HIGH);
	CCircuitAI::CircuitDefs& defs = circuit->GetCircuitDefs();
	for (auto& kv : defs) {
		CCircuitDef* cdef = kv.second;
		UnitDef* def = cdef->GetUnitDef();
		if ((def->GetSpeed() == 0) && (blockInfos.find(kv.first) == blockInfos.end())) {
			ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
			bsize = ssize + int2(4, 4);
			blockInfos[cdef->GetId()] = new CBlockRectangle(offset, bsize, ssize, SBlockingMap::StructType::UNKNOWN, ignoreMask);
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

#ifdef DEBUG_VIS
	if (dbgTextureId >= 0) {
		circuit->GetDebugDrawer()->DelOverlayTexture(dbgTextureId);
		circuit->GetDebugDrawer()->DelSDLWindow(sdlWindowId);
		delete[] dbgMap;
	}
#endif
}

int CTerrainManager::GetTerrainWidth()
{
	return terrainWidth;
}

int CTerrainManager::GetTerrainHeight()
{
	return terrainHeight;
}

void CTerrainManager::AddBlocker(CCircuitDef* cdef, const AIFloat3& pos, int facing)
{
	SStructure building = {-1, cdef, pos, facing};
	MarkBlocker(building, true);

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CTerrainManager::RemoveBlocker(CCircuitDef* cdef, const AIFloat3& pos, int facing)
{
	SStructure building = {-1, cdef, pos, facing};
	MarkBlocker(building, false);

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CTerrainManager::ResetBuildFrame()
{
	markFrame = -FRAMES_PER_SEC;
}

AIFloat3 CTerrainManager::FindBuildSite(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing)
{
	TerrainPredicate predicate = [](const AIFloat3& p) {
		return true;
	};
	return FindBuildSite(cdef, pos, searchRadius, facing, predicate);
}

AIFloat3 CTerrainManager::FindBuildSite(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing, TerrainPredicate& predicate)
{
	if (circuit->IsAllyAware()) {
		MarkAllyBuildings();
	}

	auto search = blockInfos.find(cdef->GetId());
	if (search != blockInfos.end()) {
		return FindBuildSiteByMask(cdef, pos, searchRadius, facing, search->second, predicate);
	}

	if (searchRadius > SQUARE_SIZE * 2 * 100) {
		return FindBuildSiteLow(cdef, pos, searchRadius, facing, predicate);
	}

	/*
	 * Default FindBuildSite
	 */
	UnitDef* unitDef = cdef->GetUnitDef();
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
	const std::vector<SSearchOffset>& ofs = GetSearchOffsetTable(endr);

	const int cornerX1 = int(pos.x / (SQUARE_SIZE * 2)) - (xsize / 2);
	const int cornerZ1 = int(pos.z / (SQUARE_SIZE * 2)) - (zsize / 2);

	AIFloat3 probePos(ZeroVector);
	Map* map = circuit->GetMap();

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
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}
	markFrame = circuit->GetLastFrame();

	circuit->UpdateFriendlyUnits();
	const CAllyTeam::Units& friendlies = circuit->GetFriendlyUnits();
	int teamId = circuit->GetTeamId();
	CCircuitDef* mexDef = circuit->GetEconomyManager()->GetMexDef();

	decltype(markedAllies) prevUnits = std::move(markedAllies);
	markedAllies.clear();
	auto first1  = friendlies.begin();
	auto last1   = friendlies.end();
	auto first2  = prevUnits.begin();
	auto last2   = prevUnits.end();
	auto d_first = std::back_inserter(markedAllies);
	auto addStructure = [&d_first, mexDef, this](const CCircuitUnit* unit) {
		SStructure building;
		building.unitId = unit->GetId();
		building.cdef = unit->GetCircuitDef();
		building.pos = unit->GetUnit()->GetPos();
		building.facing = unit->GetUnit()->GetBuildingFacing();
		*d_first++ = building;
		if (*building.cdef != *mexDef) {
			MarkBlocker(building, true);
		}
	};
	auto delStructure = [mexDef, this](const SStructure& building) {
		if (*building.cdef != *mexDef) {
			MarkBlocker(building, false);
		}
	};

	// @see std::set_symmetric_difference + std::set_intersection
	while (first1 != last1) {
		const CCircuitUnit* unit = first1->second;
		if ((unit->GetUnit()->GetTeam() == teamId) || (unit->GetUnit()->GetMaxSpeed() > 0)) {
			++first1;
			continue;
		}
		if (first2 == last2) {
			addStructure(unit);  // everything else in first1..last1 is new units
			while (++first1 != last1) {
				const CCircuitUnit* unit = first1->second;
				if ((unit->GetUnit()->GetTeam() == teamId) || (unit->GetUnit()->GetMaxSpeed() > 0)) {
					continue;
				}
				addStructure(unit);
			}
			break;
		}

		if (first1->first < first2->unitId) {
			addStructure(unit);  // new unit
			++first1;  // advance friendlies
		} else {
			if (first2->unitId < first1->first) {
				delStructure(*first2);  // dead unit
			} else {
				*d_first++ = *first2;  // old unit
				++first1;  // advance friendlies
			}
            ++first2;  // advance prevUnits
		}
	}
	while (first2 != last2) {  // everything else in first2..last2 is dead units
		delStructure(*first2++);
	}
}

const CTerrainManager::SearchOffsets& CTerrainManager::GetSearchOffsetTable(int radius)
{
	static std::vector<SSearchOffset> searchOffsets;
	unsigned int size = radius * radius * 4;
	if (size > searchOffsets.size()) {
		searchOffsets.resize(size);

		for (int y = 0; y < radius * 2; y++) {
			for (int x = 0; x < radius * 2; x++) {
				SSearchOffset& i = searchOffsets[y * radius * 2 + x];

				i.dx = x - radius;
				i.dy = y - radius;
				i.qdist = i.dx * i.dx + i.dy * i.dy;
			}
		}

		auto searchOffsetComparator = [](const SSearchOffset& a, const SSearchOffset& b) {
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
				SSearchOffset& i = searchOffsets[y * radius * 2 + x];
				i.dx = x - radius;
				i.dy = y - radius;
				i.qdist = i.dx * i.dx + i.dy * i.dy;
			}
		}

		auto searchOffsetComparator = [](const SSearchOffset& a, const SSearchOffset& b) {
			return a.qdist < b.qdist;
		};
		for (int yl = 0; yl < radiusLow * 2; yl++) {
			for (int xl = 0; xl < radiusLow * 2; xl++) {
				SSearchOffsetLow& il = searchOffsetsLow[yl * radiusLow * 2 + xl];
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

		auto searchOffsetLowComparator = [](const SSearchOffsetLow& a, const SSearchOffsetLow& b) {
			return a.qdist < b.qdist;
		};
		std::sort(searchOffsetsLow.begin(), searchOffsetsLow.end(), searchOffsetLowComparator);
	}

	return searchOffsetsLow;
}

AIFloat3 CTerrainManager::FindBuildSiteLow(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing, TerrainPredicate& predicate)
{
	UnitDef* unitDef = cdef->GetUnitDef();
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

AIFloat3 CTerrainManager::FindBuildSiteByMask(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask, TerrainPredicate& predicate)
{
	int xmsize = mask->GetXSize();
	int zmsize = mask->GetZSize();
	if ((searchRadius > SQUARE_SIZE * 2 * 100) || (xmsize * zmsize > GRID_RATIO_LOW * GRID_RATIO_LOW * 9)) {
		return FindBuildSiteByMaskLow(cdef, pos, searchRadius, facing, mask, predicate);
	}

	UnitDef* unitDef = cdef->GetUnitDef();
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

AIFloat3 CTerrainManager::FindBuildSiteByMaskLow(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask, TerrainPredicate& predicate)
{
	UnitDef* unitDef = cdef->GetUnitDef();
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

void CTerrainManager::MarkBlockerByMask(const SStructure& building, bool block, IBlockMask* mask)
{
	UnitDef* unitDef = building.cdef->GetUnitDef();
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

void CTerrainManager::MarkBlocker(const SStructure& building, bool block)
{
	CCircuitDef* cdef = building.cdef;
	auto search = blockInfos.find(cdef->GetId());
	if (search != blockInfos.end()) {
		MarkBlockerByMask(building, block, search->second);
		return;
	}

	/*
	 * Default marker
	 */
	int facing = building.facing;
	const AIFloat3& pos = building.pos;

	UnitDef* unitDef = cdef->GetUnitDef();
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

void CTerrainManager::CorrectPosition(AIFloat3& position)
{
	if (position.x < 1) {
		position.x = 1;
	} else if (position.x > terrainWidth - 2) {
		position.x = terrainWidth - 2;
	}
	if (position.z < 1) {
		position.z = 1;
	} else if (position.z > terrainHeight - 2) {
		position.z = terrainHeight - 2;
	}
	position.y = circuit->GetMap()->GetElevationAt(position.x, position.z);
}

STerrainMapArea* CTerrainManager::GetCurrentMapArea(CCircuitDef* cdef, const AIFloat3& position)
{
	STerrainMapMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	if (mobileType == nullptr) {  // flying units & buildings
		return nullptr;
	}

	// other mobile units & their factories
	AIFloat3 pos = position;
	CorrectPosition(pos);
	int iS = GetSectorIndex(pos);

	STerrainMapArea* area = mobileType->sector[iS].area;
	if (area == nullptr) {
		// Case: 1) unit spawned/pushed/transported outside of valid area
		//       2) factory terraformed height around and became non-valid area
		area = GetAlternativeSector(nullptr, iS, mobileType)->area;
	}
	return area;
}

int CTerrainManager::GetSectorIndex(const AIFloat3& position)
{
	return terrainData->GetSectorIndex(position);
}

bool CTerrainManager::CanMoveToPos(STerrainMapArea* area, const AIFloat3& destination)
{
	int iS = GetSectorIndex(destination);
	if (!terrainData->IsSectorValid(iS)) {
		return false;
	}
	if (area == nullptr) {  // either a flying unit or a unit was somehow created at an impossible position
		return true;
	}
	if (area == GetSectorList(area)[iS].area) {
		return true;
	}
	return false;
}

AIFloat3 CTerrainManager::GetBuildPosition(CCircuitDef* cdef, const AIFloat3& position)
{
	AIFloat3 pos = position;
	CorrectPosition(pos);
	int iS = GetSectorIndex(pos);

	STerrainMapMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	STerrainMapImmobileType* immobileType = GetImmobileTypeById(cdef->GetImmobileId());
	if (mobileType != nullptr) {  // a factory or mobile unit
		STerrainMapAreaSector* AS = GetAlternativeSector(nullptr, iS, mobileType);
		if (immobileType != nullptr) {  // a factory
			STerrainMapSector* sector = GetAlternativeSector(AS->area, iS, immobileType);
			return (sector == nullptr) ? -RgtVector : sector->position;
		} else {
			return AS->S->position;
		}
	} else if (immobileType != nullptr) {  // buildings
		return GetClosestSector(immobileType, iS)->position;
	} else {
		return pos;  // flying units
	}
}

std::vector<STerrainMapAreaSector>& CTerrainManager::GetSectorList(STerrainMapArea* sourceArea)
{
	if ((sourceArea == nullptr) ||( sourceArea->mobileType == nullptr)) {  // It flies or it's immobile
		return areaData->sectorAirType;
	}
	return sourceArea->mobileType->sector;
}

STerrainMapAreaSector* CTerrainManager::GetClosestSector(STerrainMapArea* sourceArea, const int destinationSIndex)
{
	auto iAS = sourceArea->sectorClosest.find(destinationSIndex);
	if (iAS != sourceArea->sectorClosest.end()) {  // It's already been determined
		return iAS->second;
	}

	std::vector<STerrainMapAreaSector>& TMSectors = GetSectorList(sourceArea);
	if (sourceArea == TMSectors[destinationSIndex].area) {
		sourceArea->sectorClosest[destinationSIndex] = &TMSectors[destinationSIndex];
		return &TMSectors[destinationSIndex];
	}

	AIFloat3* destination = &TMSectors[destinationSIndex].S->position;
	STerrainMapAreaSector* SClosest = nullptr;
	float sqDisClosest = std::numeric_limits<float>::max();
	for (auto& iS : sourceArea->sector) {
		float sqDist = iS.second->S->position.SqDistance2D(*destination);  // TODO: Consider SqDistance() instead of 2D
		if (sqDist < sqDisClosest) {
			SClosest = iS.second;
			sqDisClosest = sqDist;
		}
	}
	sourceArea->sectorClosest[destinationSIndex] = SClosest;
	return SClosest;
}

STerrainMapSector* CTerrainManager::GetClosestSector(STerrainMapImmobileType* sourceIT, const int destinationSIndex)
{
	auto iS = sourceIT->sectorClosest.find(destinationSIndex);
	if (iS != sourceIT->sectorClosest.end()) {  // It's already been determined
		return iS->second;
	}

	if (sourceIT->sector.find(destinationSIndex) != sourceIT->sector.end()) {
		STerrainMapSector* SClosest = &areaData->sector[destinationSIndex];
		sourceIT->sectorClosest[destinationSIndex] = SClosest;
		return SClosest;
	}

	const AIFloat3* destination = &areaData->sector[destinationSIndex].position;
	STerrainMapSector* SClosest = nullptr;
	float sqDisClosest = std::numeric_limits<float>::max();
	for (auto& iS : sourceIT->sector) {
		float sqDist = iS.second->position.SqDistance2D(*destination);  // TODO: Consider SqDistance() instead of 2D
		if (sqDist < sqDisClosest) {
			SClosest = iS.second;
			sqDisClosest = sqDist;
		}
	}
	sourceIT->sectorClosest[destinationSIndex] = SClosest;
	return SClosest;
}

STerrainMapAreaSector* CTerrainManager::GetAlternativeSector(STerrainMapArea* sourceArea, const int sourceSIndex, STerrainMapMobileType* destinationMT)
{
	std::vector<STerrainMapAreaSector>& TMSectors = GetSectorList(sourceArea);
	auto iMS = TMSectors[sourceSIndex].sectorAlternativeM.find(destinationMT);
	if (iMS != TMSectors[sourceSIndex].sectorAlternativeM.end()) {  // It's already been determined
		return iMS->second;
	}

	if (destinationMT == nullptr) {  // flying unit movetype
		return &TMSectors[sourceSIndex];
	}

	if ((sourceArea != nullptr) && (sourceArea != TMSectors[sourceSIndex].area)) {
		return GetAlternativeSector(sourceArea, GetSectorIndex(GetClosestSector(sourceArea, sourceSIndex)->S->position), destinationMT);
	}

	const AIFloat3& position = TMSectors[sourceSIndex].S->position;
	STerrainMapAreaSector* bestAS = nullptr;
	STerrainMapArea* largestArea = destinationMT->areaLargest;
	float bestDistance = -1.0;
	float bestMidDistance = -1.0;
	const std::list<STerrainMapArea*>& TMAreas = destinationMT->area;
	for (auto area : TMAreas) {
		if (area->areaUsable || !largestArea->areaUsable) {
			STerrainMapAreaSector* CAS = GetClosestSector(area, sourceSIndex);
			float midDistance; // how much of a gap exists between the two areas (source & destination)
			if ((sourceArea == nullptr) || (sourceArea == TMSectors[GetSectorIndex(CAS->S->position)].area)) {
				midDistance = 0.0;
			} else {
				midDistance = CAS->S->position.distance2D(GetClosestSector(sourceArea, GetSectorIndex(CAS->S->position))->S->position);
			}
			if ((bestMidDistance < 0) || (midDistance < bestMidDistance)) {
				bestMidDistance = midDistance;
				bestAS = nullptr;
				bestDistance = -1.0;
			}
			if (midDistance == bestMidDistance) {
				float distance = position.distance2D(CAS->S->position);
				if ((bestAS == nullptr) || (distance * area->percentOfMap < bestDistance * bestAS->area->percentOfMap)) {
					bestAS = CAS;
					bestDistance = distance;
				}
			}
		}
	}

	TMSectors[sourceSIndex].sectorAlternativeM[destinationMT] = bestAS;
	return bestAS;
}

STerrainMapSector* CTerrainManager::GetAlternativeSector(STerrainMapArea* destinationArea, const int sourceSIndex, STerrainMapImmobileType* destinationIT)
{
	std::vector<STerrainMapAreaSector>& TMSectors = GetSectorList(destinationArea);
	auto iMS = TMSectors[sourceSIndex].sectorAlternativeI.find(destinationIT);
	if (iMS != TMSectors[sourceSIndex].sectorAlternativeI.end()) {  // It's already been determined
		return iMS->second;
	}

	STerrainMapSector* closestS = nullptr;
	if (destinationArea != nullptr) {
		if (destinationArea != TMSectors[sourceSIndex].area) {
			closestS = GetAlternativeSector(destinationArea, GetSectorIndex(GetClosestSector(destinationArea, sourceSIndex)->S->position), destinationIT);
		} else {
			const AIFloat3& position = areaData->sector[sourceSIndex].position;
			float closestDistance = std::numeric_limits<float>::max();
			for (auto& iS : destinationArea->sector) {
				float sqDist = iS.second->S->position.SqDistance2D(position);  // TODO: Consider SqDistance() instead of 2D
				if (sqDist < closestDistance) {
					closestS = iS.second->S;
					closestDistance = sqDist;
				}
			}
		}
	}

	TMSectors[sourceSIndex].sectorAlternativeI[destinationIT] = closestS;
	return closestS;
}

const STerrainMapSector& CTerrainManager::GetSector(int sIndex) const
{
	return areaData->sector[sIndex];
}

STerrainMapMobileType* CTerrainManager::GetMobileType(CCircuitDef::Id unitDefId) const
{
	return GetMobileTypeById(terrainData->udMobileType[unitDefId]);
}

STerrainMapMobileType::Id CTerrainManager::GetMobileTypeId(CCircuitDef::Id unitDefId) const
{
	return terrainData->udMobileType[unitDefId];
}

STerrainMapMobileType* CTerrainManager::GetMobileTypeById(STerrainMapMobileType::Id id) const
{
	return (id < 0) ? nullptr : &areaData->mobileType[id];
}

STerrainMapImmobileType* CTerrainManager::GetImmobileType(CCircuitDef::Id unitDefId) const
{
	return GetImmobileTypeById(terrainData->udImmobileType[unitDefId]);
}

STerrainMapImmobileType::Id CTerrainManager::GetImmobileTypeId(CCircuitDef::Id unitDefId) const
{
	return terrainData->udMobileType[unitDefId];
}

STerrainMapImmobileType* CTerrainManager::GetImmobileTypeById(STerrainMapImmobileType::Id id) const
{
	return (id < 0) ? nullptr : &areaData->immobileType[id];
}

bool CTerrainManager::CanBeBuiltAt(CCircuitDef* cdef, const AIFloat3& position, const float& range)
{
	if (circuit->GetThreatMap()->GetThreatAt(position) > MIN_THREAT) {
		return false;
	}
	int iS = GetSectorIndex(position);
	STerrainMapSector* sector;
	STerrainMapMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	STerrainMapImmobileType* immobileType = GetImmobileTypeById(cdef->GetImmobileId());
	if (mobileType != nullptr) {  // a factory or mobile unit
		STerrainMapAreaSector* AS = GetAlternativeSector(nullptr, iS, mobileType);
		if (immobileType != nullptr) {  // a factory
			sector = GetAlternativeSector(AS->area, iS, immobileType);
			if (sector == nullptr) {
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
	if (circuit->GetThreatMap()->GetThreatAt(destination) > MIN_THREAT) {
		return false;
	}
	if (unit->GetCircuitDef()->GetImmobileId() != -1) {  // A hub or factory
		return unit->GetUnit()->GetPos().distance2D(destination) < unit->GetCircuitDef()->GetBuildDistance();
	}
	STerrainMapArea* area = unit->GetArea();
	if (area == nullptr) {  // A flying unit
		return true;
	}
	int iS = GetSectorIndex(destination);
	if (area->sector.find(iS) != area->sector.end()) {
		return true;
	}
	return GetClosestSector(area, iS)->S->position.distance2D(destination) < unit->GetCircuitDef()->GetBuildDistance();
}

bool CTerrainManager::CanMobileBuildAt(STerrainMapArea* area, CCircuitDef* builderDef, const AIFloat3& destination)
{
	if (circuit->GetThreatMap()->GetThreatAt(destination) > MIN_THREAT) {
		return false;
	}
	if (area == nullptr) {  // A flying unit
		return true;
	}
	int iS = GetSectorIndex(destination);
	if (area->sector.find(iS) != area->sector.end()) {
		return true;
	}
	return GetClosestSector(area, iS)->S->position.distance2D(destination) < builderDef->GetBuildDistance();
}

void CTerrainManager::UpdateAreaUsers()
{
	areaData = terrainData->GetNextAreaData();
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;

		STerrainMapArea* area = nullptr;  // flying units & buildings
		STerrainMapMobileType* mobileType = GetMobileTypeById(unit->GetCircuitDef()->GetMobileId());
		if (mobileType != nullptr) {
			// other mobile units & their factories
			AIFloat3 pos = unit->GetUnit()->GetPos();
			CorrectPosition(pos);
			int iS = GetSectorIndex(pos);

			area = mobileType->sector[iS].area;
			if (area == nullptr) {  // unit outside of valid area
				// TODO: Rescue operation
				area = GetAlternativeSector(nullptr, iS, mobileType)->area;
			}
		}
		unit->SetArea(area);
	}
	// TODO: Use boost signals to invoke UpdateAreaUsers event?
	circuit->GetBuilderManager()->UpdateAreaUsers();

	DidUpdateAreaUsers();
}

void CTerrainManager::DidUpdateAreaUsers()
{
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
	MoveData* moveDef = circuit->GetCircuitDef("armrectr")->GetUnitDef()->GetMoveData();
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
	float maxDistance = circuit->GetCircuitDef("cordoom")->GetUnitDef()->GetMaxWeaponRange() * 2;
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

#ifdef DEBUG_VIS
void CTerrainManager::UpdateVis()
{
	if (dbgTextureId >= 0) {
		for (int i = 0; i < blockingMap.gridLow.size(); ++i) {
			dbgMap[i] = (blockingMap.gridLow[i].blockerMask > 0) ? 1.0f : 0.0f;
		}
		circuit->GetDebugDrawer()->UpdateOverlayTexture(dbgTextureId, dbgMap, 0, 0, blockingMap.columnsLow, blockingMap.rowsLow);
		circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {220, 220, 0, 0});
	}
}

void CTerrainManager::ToggleVis()
{
	if (dbgTextureId < 0) {
		// /cheat
		// /debugdrawai
		// /team N
		// /spectator
		// "~block"
		dbgMap = new float [blockingMap.gridLow.size()];
		for (int i = 0; i < blockingMap.gridLow.size(); ++i) {
			dbgMap[i] = (blockingMap.gridLow[i].blockerMask > 0) ? 1.0f : 0.0f;
		}
		dbgTextureId = circuit->GetDebugDrawer()->AddOverlayTexture(dbgMap, blockingMap.columnsLow, blockingMap.rowsLow);
		circuit->GetDebugDrawer()->SetOverlayTexturePos(dbgTextureId, 0.50f, 0.25f);
		circuit->GetDebugDrawer()->SetOverlayTextureSize(dbgTextureId, 0.40f, 0.40f);
		circuit->GetDebugDrawer()->SetOverlayTextureLabel(dbgTextureId, "Blocking Map");

		std::string label = utils::int_to_string(circuit->GetSkirmishAIId(), "Circuit AI [%i] :: Blocking Map (low)");
		sdlWindowId = circuit->GetDebugDrawer()->AddSDLWindow(blockingMap.columnsLow, blockingMap.rowsLow, label.c_str());
		circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {220, 220, 0, 0});
	} else {
		circuit->GetDebugDrawer()->DelOverlayTexture(dbgTextureId);
		circuit->GetDebugDrawer()->DelSDLWindow(sdlWindowId);
		dbgTextureId = sdlWindowId = -1;
		delete[] dbgMap;
	}
}
#endif

} // namespace circuit
