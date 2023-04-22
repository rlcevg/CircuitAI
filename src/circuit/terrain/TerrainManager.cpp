/*
 * TerrainManager.cpp
 *
 *  Created on: Dec 6, 2014
 *      Author: rlcevg
 */

#include "terrain/TerrainManager.h"
#include "terrain/BlockRectangle.h"
#include "terrain/BlockCircle.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryPathWide.h"
#include "map/ThreatMap.h"
#include "map/InfluenceMap.h"
#include "module/EconomyManager.h"
#include "module/BuilderManager.h"  // Only for UpdateAreaUsers
#include "resource/MetalManager.h"
#include "resource/EnergyManager.h"
#include "scheduler/Scheduler.h"
#include "setup/SetupManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"
#include "json/json.h"

#include "spring/SpringMap.h"

#include "OOAICallback.h"
#include "WeaponDef.h"
#include "Pathing.h"
#include "MoveData.h"
#include "Log.h"

namespace circuit {

using namespace springai;
using namespace terrain;

CTerrainManager::CTerrainManager(CCircuitAI* circuit, CTerrainData* terrainData)
		: circuit(circuit)
		, terrainData(terrainData)
#ifdef DEBUG_VIS
		, dbgTextureId(-1)
		, sdlWindowId(-1)
		, dbgMap(nullptr)
#endif
{
	assert(terrainData->IsInitialized());
	terrainData->AnalyzeMap(circuit);

	areaData = terrainData->pAreaData.load();

	ResetBuildFrame();

	CMap* map = circuit->GetMap();
	int mapWidth = map->GetWidth();
	int mapHeight = map->GetHeight();
	blockingMap.columns = mapWidth / 2;  // build-step = 2 little green squares
	blockingMap.rows = mapHeight / 2;
	SBlockingMap::SBlockCell cell = {0};
	blockingMap.grid.resize(blockingMap.columns * blockingMap.rows, cell);
	blockingMap.columnsLow = mapWidth / (GRID_RATIO_LOW * 2);
	blockingMap.rowsLow = mapHeight / (GRID_RATIO_LOW * 2);
	SBlockingMap::SBlockCellLow cellLow = {0};
	blockingMap.gridLow.resize(blockingMap.columnsLow * blockingMap.rowsLow, cellLow);

	ReadConfig();
}

CTerrainManager::~CTerrainManager()
{
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

void CTerrainManager::ReadConfig()
{
	/*
	 * Building masks
	 */
	struct SBlockDesc {
		SBlockingMap::StructType structType;
		int structIdx;
		int2 offset;
		int2 yard;
		int radius;
		int radIdx;
		int2 ssize;
		int sizeIdx;
		int ignoreMask;
	};

	const Json::Value& root = circuit->GetSetupManager()->GetConfig();
	const std::string& cfgName = circuit->GetSetupManager()->GetConfigName();
	const Json::Value& block = root["building"];
	SBlockingMap::StructTypes& structTypes = SBlockingMap::GetStructTypes();
	SBlockingMap::StructMasks& structMasks = SBlockingMap::GetStructMasks();
	const std::array<std::string, 2> blockNames = {"rectangle", "circle"};
	enum {RECTANGLE = 0, CIRCLE};
	const std::array<std::string, 2> radNames = {"explosion", "expl_ally"};
	enum {EXPLOSION = 0, EXPL_ALLY};
	minLandPercent = root["select"].get("min_land", 40.0f).asFloat();
	const bool isWaterMap = IsWaterMap();

	const Json::Value& clLand = block["class_land"];
	const Json::Value& clWater = block["class_water"];
	const Json::Value& instance = block["instance"];

	auto readBlockDesc = [this, &cfgName, &structTypes, &structMasks, &blockNames, &radNames]
						 (const char* clName, const Json::Value& cls, SBlockDesc& outDesc)
	{
		const Json::Value& type = cls["type"];
		const std::string& strST = type.get((unsigned)1, "").asString();
		auto it = structTypes.find(strST);
		if (it == structTypes.end()) {
			circuit->LOG("CONFIG %s: '%s' has unknown struct type '%s'", cfgName.c_str(), clName, strST.c_str());
			return false;
		}
		outDesc.structType = it->second;

		outDesc.structIdx = -1;
		const std::string& strBT = type.get((unsigned)0, "").asString();
		for (unsigned i = 0; i < blockNames.size(); ++i) {
			if (strBT == blockNames[i]) {
				outDesc.structIdx = i;
				break;
			}
		}
		if (outDesc.structIdx < 0) {
			circuit->LOG("CONFIG %s: '%s' has unknown block type '%s'", cfgName.c_str(), clName, strBT.c_str());
			return false;
		}

		const Json::Value& offs = cls["offset"];
		outDesc.offset = int2(offs.get((unsigned)0, 0).asInt(), offs.get((unsigned)1, 0).asInt());

		outDesc.radIdx = -1;
		switch (outDesc.structIdx) {
			default:
			case RECTANGLE: {
				const Json::Value& rd = cls["yard"];
				outDesc.yard = int2(rd.get((unsigned)0, 0).asInt(), rd.get((unsigned)1, 0).asInt());
			} break;
			case CIRCLE: {
				const Json::Value& rad = cls["radius"];
				if (rad.empty()) {
					outDesc.radIdx = EXPLOSION;
				} else if (rad.isString()) {
					const std::string& strRad = rad.asString();
					for (unsigned i = 0; i < radNames.size(); ++i) {
						if (strRad == radNames[i]) {
							outDesc.radIdx = i;
							break;
						}
					}
					if (outDesc.radIdx < 0) {
						circuit->LOG("CONFIG %s: '%s' has unknown radius '%s'", cfgName.c_str(), clName, strRad.c_str());
						outDesc.radIdx = EXPLOSION;
					}
				} else {
					outDesc.radius = rad.asInt();
				}
			} break;
		}

		outDesc.sizeIdx = -1;
		const Json::Value& size = cls["size"];
		if (!size.empty()) {
			outDesc.ssize = int2(size.get((unsigned)0, 0).asInt(), size.get((unsigned)1, 0).asInt());
			outDesc.sizeIdx = 0;
		}

		outDesc.ignoreMask = STRUCT_BIT(NONE);
		const Json::Value& ignore = cls["ignore"];
		if (!ignore.empty()) {
			for (const Json::Value& mask : ignore) {
				auto it = structMasks.find(mask.asString());
				if (it == structMasks.end()) {
					circuit->LOG("CONFIG %s: '%s' has unknown ignore type '%s'", cfgName.c_str(), clName, mask.asCString());
				} else {
					outDesc.ignoreMask |= static_cast<SBlockingMap::SM>(it->second);
				}
			}
		} else {
			const Json::Value& notIgnore = cls["not_ignore"];
			if (!notIgnore.empty()) {
				int notIgnoreMask = STRUCT_BIT(NONE);
				for (const Json::Value& mask : notIgnore) {
					auto it = structMasks.find(mask.asString());
					if (it == structMasks.end()) {
						circuit->LOG("CONFIG %s: '%s' has unknown not_ignore type '%s'", cfgName.c_str(), clName, mask.asCString());
					} else {
						notIgnoreMask |= static_cast<SBlockingMap::SM>(it->second);
					}
				}
				outDesc.ignoreMask = STRUCT_BIT(ALL) & ~notIgnoreMask;
			}
		}

		return true;
	};

	auto createBlockInfo = [this](SBlockDesc& blockDesc, UnitDef* def) {
		if (blockDesc.sizeIdx < 0) {
			blockDesc.ssize = int2(def->GetXSize() / 2, def->GetZSize() / 2);
		}

		IBlockMask* blocker;
		switch (blockDesc.structIdx) {
			default:
			case RECTANGLE: {
				int2 bsize = blockDesc.ssize + blockDesc.yard;
				blocker = new CBlockRectangle(blockDesc.offset, bsize, blockDesc.ssize, blockDesc.structType, blockDesc.ignoreMask);
			} break;
			case CIRCLE: {
				switch (blockDesc.radIdx) {
					case EXPLOSION: {
						WeaponDef* wpDef = def->GetDeathExplosion();
						blockDesc.radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
						delete wpDef;
					} break;
					case EXPL_ALLY: {
						WeaponDef* wpDef = def->GetDeathExplosion();
						blockDesc.radius = wpDef->GetAreaOfEffect() / (SQUARE_SIZE * 2);
						// [radius ~ 1 player ; radius/2 ~ 4+ players]
						blockDesc.radius -= blockDesc.radius / 6 * (std::min(circuit->GetAllyTeam()->GetSize(), 4) - 1);
						delete wpDef;
					} break;
					default: break;
				}
				blocker = new CBlockCircle(blockDesc.offset, blockDesc.radius, blockDesc.ssize, blockDesc.structType, blockDesc.ignoreMask);
			} break;
		}
		return blocker;
	};

	for (const std::string& clName : instance.getMemberNames()) {
		Json::Value cls = isWaterMap ? clWater[clName] : Json::Value::nullSingleton();
		if (cls.empty()) {
			cls = clLand[clName];
			if (cls.empty()) {
				circuit->LOG("CONFIG %s: unknown instances of class '%s'", cfgName.c_str(), clName.c_str());
				continue;
			}
		}

		SBlockDesc blockDesc;
		if (!readBlockDesc(clName.c_str(), cls, blockDesc)) {
			continue;
		}

		const Json::Value& defNames = instance[clName];
		for (const Json::Value& def : defNames) {
			CCircuitDef* cdef = circuit->GetCircuitDef(def.asCString());
			if (cdef == nullptr) {
				circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), def.asCString());
				continue;
			}

			if (blockInfos.find(cdef->GetId()) != blockInfos.end()) {
				circuit->LOG("CONFIG %s: ignored block_map duplicate of '%s'", cfgName.c_str(), def.asCString());
				continue;
			}

			blockInfos[cdef->GetId()] = createBlockInfo(blockDesc, cdef->GetDef());
		}
	}

	SBlockDesc blockDesc;
	const char* defName = "_default_";
	if (readBlockDesc(defName, clLand[defName], blockDesc)) {
		for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
			if (!cdef.IsMobile() && (blockInfos.find(cdef.GetId()) == blockInfos.end())) {
				blockInfos[cdef.GetId()] = createBlockInfo(blockDesc, cdef.GetDef());
			}
		}
	}
}

void CTerrainManager::Init()
{
	const CMetalData::Metals& mspots = circuit->GetMetalManager()->GetSpots();
	CCircuitDef* cdef = circuit->GetEconomyManager()->GetSideInfo().mexDef;
	int xsize, zsize;
	auto it = blockInfos.find(cdef->GetId());
	if (it != blockInfos.end()) {
		xsize = it->second->GetXSize();
		zsize = it->second->GetZSize();
	} else {
		xsize = cdef->GetDef()->GetXSize() / 2;
		zsize = cdef->GetDef()->GetZSize() / 2;
	}
	int notIgnoreMask = ~STRUCT_BIT(MEX);  // all except mex
	for (auto& spot : mspots) {
		const int x1 = int(spot.position.x / (SQUARE_SIZE << 1)) - (xsize >> 1), x2 = x1 + xsize;
		const int z1 = int(spot.position.z / (SQUARE_SIZE << 1)) - (zsize >> 1), z2 = z1 + zsize;
		int2 m1(x1, z1);
		int2 m2(x2, z2);
		blockingMap.Bound(m1, m2);
		for (int z = m1.y; z < m2.y; ++z) {
			for (int x = m1.x; x < m2.x; ++x) {
				blockingMap.MarkBlocker(x, z, SBlockingMap::StructType::MEX, notIgnoreMask);
			}
		}
	}

	const CEnergyData::Geos& espots = circuit->GetEnergyManager()->GetSpots();
	cdef = circuit->GetEconomyManager()->GetSideInfo().geoDef;
	it = blockInfos.find(cdef->GetId());
	if (it != blockInfos.end()) {
		xsize = it->second->GetXSize();
		zsize = it->second->GetZSize();
	} else {
		xsize = cdef->GetDef()->GetXSize() / 2;
		zsize = cdef->GetDef()->GetZSize() / 2;
	}
	notIgnoreMask = ~STRUCT_BIT(GEO);  // all except geo
	for (auto& spot : espots) {
		const int x1 = int(spot.x / (SQUARE_SIZE << 1)) - (xsize >> 1), x2 = x1 + xsize;
		const int z1 = int(spot.z / (SQUARE_SIZE << 1)) - (zsize >> 1), z2 = z1 + zsize;
		int2 m1(x1, z1);
		int2 m2(x2, z2);
		blockingMap.Bound(m1, m2);
		for (int z = m1.y; z < m2.y; ++z) {
			for (int x = m1.x; x < m2.x; ++x) {
				blockingMap.MarkBlocker(x, z, SBlockingMap::StructType::GEO, notIgnoreMask);
			}
		}
	}

	// Mark edges of the map
	notIgnoreMask = STRUCT_BIT(NONE);
	for (int j = 0; j < 4; ++j) {
		for (int i = 5; i < blockingMap.columns - 5; ++i) {
			blockingMap.MarkBlocker(i, j, SBlockingMap::StructType::TERRA, notIgnoreMask);
			blockingMap.MarkBlocker(i, blockingMap.rows - j - 1, SBlockingMap::StructType::TERRA, notIgnoreMask);
		}
	}
	for (int j = 5; j < blockingMap.rows - 5; ++j) {
		for (int i = 0; i < 4; ++i) {
			blockingMap.MarkBlocker(i, j, SBlockingMap::StructType::TERRA, notIgnoreMask);
			blockingMap.MarkBlocker(blockingMap.columns - i - 1, j, SBlockingMap::StructType::TERRA, notIgnoreMask);
		}
	}
}

void CTerrainManager::AddBlocker(CCircuitDef* cdef, const AIFloat3& pos, int facing, bool isOffset)
{
	AIFloat3 newPos = pos;
	if (isOffset) {
		newPos += cdef->GetMidPosOffset(facing);
	}

	SStructure building = {-1, cdef, newPos, facing};
	MarkBlocker(building, true);

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

void CTerrainManager::DelBlocker(CCircuitDef* cdef, const AIFloat3& pos, int facing, bool isOffset)
{
	AIFloat3 newPos = pos;
	if (isOffset) {
		newPos += cdef->GetMidPosOffset(facing);
	}

	SStructure building = {-1, cdef, newPos, facing};
	MarkBlocker(building, false);

#ifdef DEBUG_VIS
	UpdateVis();
#endif
}

bool CTerrainManager::IsObstruct(const AIFloat3& pos) const
{
	const int x = int(pos.x + 0.5f) / (SQUARE_SIZE * 2);
	const int z = int(pos.z + 0.5f) / (SQUARE_SIZE * 2);

	return blockingMap.IsStruct(x, z);
}

//AIFloat3 CTerrainManager::CheckObstruct(CCircuitUnit* unit) const
//{
//	/*
//	 * Check 4 directions WRT unit radius and find shortest direction out of struct
//	 */
//	// FIXME: false positives on solars: no way to distinguish between plan and building.
//	//        Short path out of struct isn't towards build-target or in free space -
//	//        may stuck going back and forth between 2 close buildings.
//	if (unit->GetCircuitDef()->GetMobileId() < 0) {
//		return -RgtVector;
//	}
//
//	const AIFloat3& pos = unit->GetPos(circuit->GetLastFrame());
//	const int2 c(int(pos.x + 0.5f) / (SQUARE_SIZE * 2), int(pos.z + 0.5f) / (SQUARE_SIZE * 2));
//	const int radius = unit->GetCircuitDef()->GetRadius() / (SQUARE_SIZE * 2);
//	const std::array<int2, 4> exts = {
//		int2(0, radius),  // UNIT_FACING_SOUTH
//		int2(radius, 0),  // UNIT_FACING_EAST
//		int2(0, -radius),  // UNIT_FACING_NORTH
//		int2(-radius, 0)  // UNIT_FACING_WEST
//	};
//	const std::array<int2, 4> dirs = {
//		int2(0, -1),  // UNIT_FACING_SOUTH
//		int2(-1, 0),  // UNIT_FACING_EAST
//		int2(0, 1),  // UNIT_FACING_NORTH
//		int2(1, 0)  // UNIT_FACING_WEST
//	};
//
//	std::array<int2, 4> cs = {
//		c + exts[UNIT_FACING_SOUTH],
//		c + exts[UNIT_FACING_EAST],
//		c + exts[UNIT_FACING_NORTH],
//		c + exts[UNIT_FACING_WEST]
//	};
//	std::vector<int> facings;
//	facings.reserve(4);
//	for (int facing = 0; facing < 4; ++facing) {
//		const int2 t = cs[facing];
//		if (blockingMap.IsInBounds(t.x, t.y) && blockingMap.IsStruct(t.x, t.y)) {
//			facings.push_back(facing);
//		}
//	}
//	if (facings.empty()) {
//		return -RgtVector;
//	}
//
//	int facing = UNIT_NO_FACING;
//	int numTry = 0;
//	constexpr int NUM_TRIES = 32;
//	do {
//		for (int f : facings) {
//			const int2 t = (cs[f] += dirs[f]);
//			if (blockingMap.IsInBounds(t.x, t.y) && !blockingMap.IsStruct(t.x, t.y)) {
//				facing = f;
//				break;
//			}
//		}
//	} while ((facing == UNIT_NO_FACING) && (++numTry < NUM_TRIES));
//
//	const int2 t = cs[facing] - exts[facing] + dirs[facing];  // +dirs[facing] is slack, engine ignores short move-order
//	if ((numTry >= NUM_TRIES) || !blockingMap.IsInBounds(t.x, t.y)) {
//		return utils::get_radial_pos(pos, 64.f);
//	}
//
//	return AIFloat3(t.x * (SQUARE_SIZE * 2) + SQUARE_SIZE, 0, t.y * (SQUARE_SIZE * 2) + SQUARE_SIZE);
//}

void CTerrainManager::AddBusPath(CCircuitUnit* unit, const AIFloat3& toPos, CCircuitDef* mobileDef)
{
	AIFloat3 startPos = unit->GetPos(circuit->GetLastFrame());
	bool isOK;
	SArea* area;
	std::tie(area, isOK) = GetCurrentMapArea(mobileDef, startPos);
	if (!isOK) {
		return;
	}
	const int iS = GetSectorIndex(toPos);
	// FIXME: altitude works only for armcom (amphibious) move-type;
	//        create additional for land, and for water?
	constexpr int altitude = 4 * 2 * ALTITUDE_SCALE;  // 4 - tiles in sector, side
	SAreaSector* CAS = GetClosestSectorWithAltitude(area, iS, altitude);
	if (CAS == nullptr) {
		return;
	}

	AIFloat3 sectorStep;
	switch (unit->GetUnit()->GetBuildingFacing()) {
		default:
		case UNIT_FACING_SOUTH: {
			// FIXME: startPos is aimpoint, not center
			const int edgeZH = unit->GetCircuitDef()->GetDef()->GetZSize() * (SQUARE_SIZE / 2) + SQUARE_SIZE * 5;
			sectorStep = AIFloat3(0.f, 0.f, GetConvertStoP());
			startPos += AIFloat3(0.f, 0.f, edgeZH);
		} break;
		case UNIT_FACING_EAST: {
			const int edgeXH = unit->GetCircuitDef()->GetDef()->GetXSize() * (SQUARE_SIZE / 2) + SQUARE_SIZE * 5;
			sectorStep = AIFloat3(GetConvertStoP(), 0.f, 0.f);
			startPos += AIFloat3(edgeXH, 0.f, 0.f);
		} break;
		case UNIT_FACING_NORTH: {
			const int edgeZH = unit->GetCircuitDef()->GetDef()->GetZSize() * (SQUARE_SIZE / 2) + SQUARE_SIZE * 5;
			sectorStep = AIFloat3(0.f, 0.f, -GetConvertStoP());
			startPos += AIFloat3(0.f, 0.f, -edgeZH);
		} break;
		case UNIT_FACING_WEST: {
			const int edgeXH = unit->GetCircuitDef()->GetDef()->GetXSize() * (SQUARE_SIZE / 2) + SQUARE_SIZE * 5;
			sectorStep = AIFloat3(-GetConvertStoP(), 0.f, 0.f);
			startPos += AIFloat3(-edgeXH, 0.f, 0.f);
		} break;
	}
	if (!utils::is_in_map(startPos) || !CanMoveToPos(area, startPos)) {
		startPos -= sectorStep;
	}

	const AIFloat3& endPos = CAS->S->position;

	// NOTE: heuristic in micropather may lead not to closest node, but a bit further,
	//       as it tests only single end-node.
	//       Reduce end-nodes in half (interleave) and test manhattan distance to each?
	IndexVec targets;
	for (const auto& kv : busPath) {
		if (kv.second != nullptr) {
			// targets will have many duplicates, but performance hit shouldn't
			// worth an effort to store additional array of only unique sectors
			// @see FillParentBusNodes
			targets.insert(targets.end(), kv.second->path.begin(), kv.second->path.end());
		}
	}

	FactoryPathQuery& fpq = busQueries[unit];
	fpq.mobileDef = mobileDef;
	fpq.startPos = startPos;
	fpq.endPos = endPos;
	fpq.targets = std::move(targets);
	busPath[unit] = nullptr;
}

void CTerrainManager::DelBusPath(CCircuitUnit* unit)
{
	auto it = busPath.find(unit);
	if (it == busPath.end()) {
		return;
	}

	if (it->second != nullptr) {
		// TODO: Path for new factories contains only part that connects to 1st built path.
		//       Hence removing it leaves others with short leftover.
		//       Place it to AllyTeam and count or copy common path nodes.
		CPathFinder* pathfinder = circuit->GetPathfinder();
		const int granularity = pathfinder->GetSquareSize() / (SQUARE_SIZE * 2);
		const int howWide = pathfinder->GetSquareSize() / 32;
		for (int index : it->second->path) {
			int ix, iz;
			pathfinder->PathIndex2PathXY(index, &ix, &iz);

			ix = ix * granularity + granularity / 2;
			iz = iz * granularity + granularity / 2;
			int2 m1 = (howWide & 1) ? int2(ix - 0, iz - 0) : int2(ix - 3, iz - 3);
			int2 m2 = (howWide & 1) ? int2(ix + 6, iz + 6) : int2(ix + 3, iz + 3);
			blockingMap.Bound(m1, m2);
			for (int z = m1.y; z < m2.y; ++z) {
				for (int x = m1.x; x < m2.x; ++x) {
					blockingMap.DelBlocker(x, z, SBlockingMap::StructType::TERRA);
				}
			}
		}
	}
	busPath.erase(it);
	busQueries.erase(unit);
}

AIFloat3 CTerrainManager::GetBusPos(CCircuitDef* facDef, const AIFloat3& pos, int& outFacing)
{
	outFacing = UNIT_NO_FACING;

	CCircuitUnit* unit = nullptr;
	CPathInfo* pathInfo = nullptr;
	const int frame = circuit->GetLastFrame();
	float minSqDist = std::numeric_limits<float>::max();
	for (auto& kv : busPath) {
		const float sqDist = kv.first->GetPos(frame).SqDistance2D(pos);
		if ((minSqDist > sqDist) && (kv.second != nullptr)) {
			minSqDist = sqDist;
			unit = kv.first;
			pathInfo = kv.second.get();
		}
	}
	if ((unit == nullptr) || pathInfo->path.empty()) {
		return pos;
	}

	/// TODO: make sorted offsets pattern for this specific case, instead of circle
	CPathFinder* pathfinder = circuit->GetPathfinder();
	const int incr = std::max(1, 64 / GetConvertStoP());  // convertStoP ~= 32, 64, 128
	const int maxIdx = std::min<int>(pathInfo->path.size(), 32 * incr);
	AIFloat3 prevPos = pathfinder->PathIndex2Pos(pathInfo->path[0]);
	const float searchRadius = std::max(facDef->GetDef()->GetZSize() * SQUARE_SIZE, GetConvertStoP()) / 2;
	const std::array<AIFloat3, 4> faceOffs = {
		AIFloat3(0, 0, -GetConvertStoP() * incr),  // UNIT_FACING_SOUTH
		AIFloat3(-GetConvertStoP() * incr, 0, 0),  // UNIT_FACING_EAST
		AIFloat3(0, 0, GetConvertStoP() * incr),  // UNIT_FACING_NORTH
		AIFloat3(GetConvertStoP() * incr, 0, 0)  // UNIT_FACING_WEST
	};
	for (int index = incr; index < maxIdx; index += incr) {
		const AIFloat3& pathPos = pathfinder->PathIndex2Pos(pathInfo->path[index]);
		std::array<int, 2> testFaces;
		if (std::fabs(pathPos.x - prevPos.x) > std::fabs(pathPos.z - prevPos.z)) {
			testFaces = {UNIT_FACING_SOUTH, UNIT_FACING_NORTH};
		} else {
			testFaces = {UNIT_FACING_EAST, UNIT_FACING_WEST};
		}
		for (int facing : testFaces) {
			AIFloat3 buildPos = pathPos + faceOffs[facing];
			CTerrainManager::CorrectPosition(buildPos);
			buildPos = FindBuildSite(facDef, buildPos, searchRadius, facing);
			if (utils::is_valid(buildPos)) {
				outFacing = facing;
				return buildPos;
			}
		}
		prevPos = pathPos;
	}
	return pos;
}

AIFloat3 CTerrainManager::FindBuildSite(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing, bool isIgnore)
{
	TerrainPredicate predicate = [](const AIFloat3& p) {
		return true;
	};
	return FindBuildSite(cdef, pos, searchRadius, facing, predicate, isIgnore);
}

AIFloat3 CTerrainManager::FindBuildSite(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing, TerrainPredicate& predicate, bool isIgnore)
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);
	if (circuit->IsAllyAware()) {
		MarkAllyBuildings();
	}

	auto search = blockInfos.find(cdef->GetId());
	if (search != blockInfos.end()) {
		return FindBuildSiteByMask(cdef, pos, searchRadius, facing, search->second, predicate);
	}

	if (searchRadius > SQUARE_SIZE * 2 * 100) {
		return FindBuildSiteLow(cdef, pos, searchRadius, facing, predicate);  // isIgnore = false
	}

	/*
	 * Default FindBuildSite
	 */
	UnitDef* unitDef = cdef->GetDef();
	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	const SBlockingMap::SM notIgnore = static_cast<SBlockingMap::SM>(isIgnore ? SBlockingMap::StructMask::NONE : SBlockingMap::StructMask::ALL);
	auto isOpenSite = [this, notIgnore](const int2& s1, const int2& s2) {
		for (int z = s1.y; z < s2.y; ++z) {
			for (int x = s1.x; x < s2.x; ++x) {
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
	CMap* map = circuit->GetMap();

	for (int so = 0; so < endr * endr * 4; so++) {
		int2 s1(cornerX1 + ofs[so].dx, cornerZ1 + ofs[so].dy);
		int2 s2(    s1.x + xsize,          s1.y + zsize);
		if (!blockingMap.IsInBounds(s1, s2) || !isOpenSite(s1, s2)) {
			continue;
		}

		probePos.x = (s1.x + s2.x) * SQUARE_SIZE;
		probePos.z = (s1.y + s2.y) * SQUARE_SIZE;
		if (CanBeBuiltAtSafe(cdef, probePos) && map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
			probePos.y = map->GetElevationAt(probePos.x, probePos.z);
			if (predicate(probePos)) {
				return probePos;
			}
		}
	}

	return -RgtVector;
}

//AIFloat3 CTerrainManager::FindSpringBuildSite(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing)
//{
//	return circuit->GetMap()->FindClosestBuildSite(cdef->GetDef(), pos, searchRadius, 0, facing);
//}

void CTerrainManager::DoLineOfDef(const AIFloat3& start, const AIFloat3& end, CCircuitDef* buildDef,
		std::function<void (const AIFloat3& pos, CCircuitDef* buildDef)> exec) const
{
	const AIFloat3 delta = end - start;
	float xsize, zsize;

	auto search = blockInfos.find(buildDef->GetId());
	if (search != blockInfos.end()) {
		xsize = search->second->GetXSize() * 2 * SQUARE_SIZE;
		zsize = search->second->GetZSize() * 2 * SQUARE_SIZE;
	} else {
		xsize = buildDef->GetDef()->GetXSize() * SQUARE_SIZE;
		zsize = buildDef->GetDef()->GetZSize() * SQUARE_SIZE;
	}
	// NOTE: Ignore facing for simplicity, hence turn rectangles into squares
	xsize = zsize = std::max(xsize, zsize);

	const int xnum = (int)((math::fabs(delta.x) + xsize * 1.4f) / xsize);
	const int znum = (int)((math::fabs(delta.z) + zsize * 1.4f) / zsize);

	float xstep = (int)((0 < delta.x) ? xsize : -xsize);
	float zstep = (int)((0 < delta.z) ? zsize : -zsize);

	const bool xDominatesZ = (math::fabs(delta.x) > math::fabs(delta.z));

	if (xDominatesZ) {
		zstep = xstep * delta.z / (delta.x ? delta.x : 1);
	} else {
		xstep = zstep * delta.x / (delta.z ? delta.z : 1);
	}

	int n = xDominatesZ ? xnum : znum, x = start.x, z = start.z;
	for (int i = 0; i < n; ++i) {
		exec(AIFloat3(x, 0.f, z), buildDef);

		x += xstep;
		z += zstep;
	}
}

const SBlockingMap& CTerrainManager::GetBlockingMap()
{
	if (circuit->IsAllyAware()) {
		MarkAllyBuildings();
	}
	return blockingMap;
}

bool CTerrainManager::ResignAllyBuilding(CCircuitUnit* unit)
{
	auto it = markedAllies.cbegin();
	while (it != markedAllies.cend()) {
		if (it->unitId == unit->GetId()) {
			markedAllies.erase(it);
			return true;
		}
		++it;
	}
	return false;
}

void CTerrainManager::ApplyAuthority()
{
	for (SStructure& building : markedAllies) {
		building.cdef = circuit->GetCircuitDef(building.cdef->GetId());
	}
}

void CTerrainManager::MarkAllyBuildings()
{
	if (markFrame /*+ FRAMES_PER_SEC*/ >= circuit->GetLastFrame()) {
		return;
	}
	markFrame = circuit->GetLastFrame();

	circuit->UpdateFriendlyUnits();
	const CAllyTeam::AllyUnits& friendlies = circuit->GetFriendlyUnits();
	const int teamId = circuit->GetTeamId();
	const int frame = circuit->GetLastFrame();

	decltype(markedAllies) prevUnits = std::move(markedAllies);
	markedAllies.clear();
	auto first1  = friendlies.begin();
	auto last1   = friendlies.end();
	auto first2  = prevUnits.begin();
	auto last2   = prevUnits.end();
	auto d_first = std::back_inserter(markedAllies);
	auto addStructure = [this, &d_first, frame](CAllyUnit* unit) {
		SStructure building;
		building.unitId = unit->GetId();
		building.cdef = unit->GetCircuitDef();
		building.facing = unit->GetUnit()->GetBuildingFacing();
		building.pos = unit->GetPos(frame) + building.cdef->GetMidPosOffset(building.facing);
		*d_first++ = building;
//		if (!building.cdef->IsMex()) {  // mex positions are marked on start and must not change
			MarkBlocker(building, true);
//		}
	};
	auto delStructure = [this](const SStructure& building) {
//		if (!building.cdef->IsMex()) {  // mex positions are marked on start and must not change
			MarkBlocker(building, false);
//		}
	};

	// @see std::set_symmetric_difference + std::set_intersection
	while (first1 != last1) {
		CAllyUnit* unit = first1->second;
		if (unit->GetCircuitDef()->IsMobile() || (unit->GetUnit()->GetTeam() == teamId)) {
			++first1;
			continue;
		}
		if (first2 == last2) {
			addStructure(unit);  // everything else in first1..last1 is new units
			while (++first1 != last1) {
				CAllyUnit* unit = first1->second;
				if (unit->GetCircuitDef()->IsMobile() || (unit->GetUnit()->GetTeam() == teamId)) {
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
				i.dx = x - radius + GRID_RATIO_LOW / 2;
				i.dy = y - radius + GRID_RATIO_LOW / 2;
				i.qdist = i.dx * i.dx + i.dy * i.dy;  // from corner low-res cell
//				i.qdist = SQUARE(x - radius) + SQUARE(y - radius);  // from center of low-res cell
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
	UnitDef* unitDef = cdef->GetDef();
	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	const SBlockingMap::SM notIgnore = static_cast<SBlockingMap::SM>(SBlockingMap::StructMask::ALL);
	auto isOpenSite = [this, notIgnore](const int2& s1, const int2& s2) {
		for (int z = s1.y; z < s2.y; z++) {
			for (int x = s1.x; x < s2.x; x++) {
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

	AIFloat3 probePos(ZeroVector);
	CMap* map = circuit->GetMap();

	for (int soLow = 0; soLow < endrLow * endrLow * 4; soLow++) {
		int xlow = centerX + ofsLow[soLow].dx;
		int zlow = centerZ + ofsLow[soLow].dy;
		if (!blockingMap.IsInBoundsLow(xlow, zlow) || blockingMap.IsBlockedLow(xlow, zlow, notIgnore)) {
			continue;
		}

		probePos.x = (xlow * 2 + 1) * SQUARE_SIZE * GRID_RATIO_LOW;
		probePos.z = (zlow * 2 + 1) * SQUARE_SIZE * GRID_RATIO_LOW;
		if (!CanBeBuiltAtSafe(cdef, probePos)) {
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
			if (CanBeBuiltAtSafe(cdef, probePos) && map->IsPossibleToBuildAt(unitDef, probePos, facing)) {
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
	if ((searchRadius > SQUARE_SIZE * 2 * 100) || (xmsize * zmsize > GRID_RATIO_LOW * GRID_RATIO_LOW * 4)) {
		return FindBuildSiteByMaskLow(cdef, pos, searchRadius, facing, mask, predicate);
	}

	UnitDef* unitDef = cdef->GetDef();
	int xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
		} break;
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
		} break;
	}

#define DECLARE_TEST(testName, facingType)																	\
	auto testName = [this, mask, notIgnore, structMask](const int2& m1, const int2& m2, const int2& om) {	\
		for (int z = m1.y, zm = om.y; z < m2.y; z++, zm++) {												\
			for (int x = m1.x, xm = om.x; x < m2.x; x++, xm++) {											\
				switch (mask->facingType(xm, zm)) {															\
					case IBlockMask::BlockType::BLOCK: {													\
						if (blockingMap.IsStructed(x, z, structMask)) {										\
							return false;																	\
						}																					\
					} break;																				\
					case IBlockMask::BlockType::STRUCT: {													\
						if (blockingMap.IsBlocked(x, z, notIgnore)) {										\
							return false;																	\
						}																					\
					} break;																				\
					case IBlockMask::BlockType::OPEN: {														\
					} break;																				\
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
	CMap* map = circuit->GetMap();

#define DO_TEST(testName)																				\
	for (int so = 0; so < endr * endr * 4; so++) {														\
		int2 s1(structCorner.x + ofs[so].dx, structCorner.y + ofs[so].dy);								\
		int2 s2(          s1.x + xssize,               s1.y + zssize);									\
		if (!blockingMap.IsInBounds(s1, s2)) {															\
			continue;																					\
		}																								\
																										\
		probePos.x = (s1.x + s2.x) * SQUARE_SIZE;														\
		probePos.z = (s1.y + s2.y) * SQUARE_SIZE;														\
		if (!CanBeBuiltAtSafe(cdef, probePos)) {														\
			continue;																					\
		}																								\
																										\
		int2 m1(maskCorner.x + ofs[so].dx, maskCorner.y + ofs[so].dy);									\
		int2 m2(        m1.x + xmsize,             m1.y + zmsize);										\
		int2 om = m1;																					\
		blockingMap.Bound(m1, m2);																		\
		om = m1 - om;																					\
		if (!testName(m1, m2, om)) {																	\
			continue;																					\
		}																								\
																										\
		if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {										\
			probePos.y = map->GetElevationAt(probePos.x, probePos.z);									\
			if (predicate(probePos)) {																	\
				return probePos;																		\
			}																							\
		}																								\
	}

	switch (facing) {
		default:
		case UNIT_FACING_SOUTH: {
			DECLARE_TEST(isOpenSouth, GetTypeSouth);
			DO_TEST(isOpenSouth);
		} break;
		case UNIT_FACING_EAST: {
			DECLARE_TEST(isOpenEast, GetTypeEast);
			DO_TEST(isOpenEast);
		} break;
		case UNIT_FACING_NORTH: {
			DECLARE_TEST(isOpenNorth, GetTypeNorth);
			DO_TEST(isOpenNorth);
		} break;
		case UNIT_FACING_WEST: {
			DECLARE_TEST(isOpenWest, GetTypeWest);
			DO_TEST(isOpenWest);
		} break;
	}
#undef DO_TEST
#undef DECLARE_TEST

	return -RgtVector;
}

AIFloat3 CTerrainManager::FindBuildSiteByMaskLow(CCircuitDef* cdef, const AIFloat3& pos, float searchRadius, int facing, IBlockMask* mask, TerrainPredicate& predicate)
{
	UnitDef* unitDef = cdef->GetDef();
	int xmsize, zmsize, xssize, zssize;
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH:
		case UNIT_FACING_NORTH: {
			xmsize = mask->GetXSize();
			zmsize = mask->GetZSize();
			xssize = unitDef->GetXSize() / 2;
			zssize = unitDef->GetZSize() / 2;
		} break;
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
		} break;
	}

#define DECLARE_TEST_LOW(testName, facingType)																\
	auto testName = [this, mask, notIgnore, structMask](const int2& m1, const int2& m2, const int2& om) {	\
		for (int z = m1.y, zm = om.y; z < m2.y; z++, zm++) {												\
			for (int x = m1.x, xm = om.x; x < m2.x; x++, xm++) {											\
				switch (mask->facingType(xm, zm)) {															\
					case IBlockMask::BlockType::BLOCK: {													\
						if (blockingMap.IsStructed(x, z, structMask)) {										\
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
					case IBlockMask::BlockType::OPEN: {														\
					} break;																				\
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
	CMap* map = circuit->GetMap();

#define DO_TEST_LOW(testName)																					\
	for (int soLow = 0; soLow < endrLow * endrLow * 4; soLow++) {												\
		int2 low(structCenter.x + ofsLow[soLow].dx, structCenter.y + ofsLow[soLow].dy);							\
		if (!blockingMap.IsInBoundsLow(low.x, low.y) || blockingMap.IsBlockedLow(low.x, low.y, notIgnore)) {	\
			continue;																							\
		}																										\
																												\
		probePos.x = (low.x * 2 + 1) * SQUARE_SIZE * GRID_RATIO_LOW;											\
		probePos.z = (low.y * 2 + 1) * SQUARE_SIZE * GRID_RATIO_LOW;											\
		if (!CanBeBuiltAtSafe(cdef, probePos)) {																\
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
			probePos.x = (s1.x + s2.x) * SQUARE_SIZE;															\
			probePos.z = (s1.y + s2.y) * SQUARE_SIZE;															\
			if (!CanBeBuiltAtSafe(cdef, probePos)) {															\
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
			if (map->IsPossibleToBuildAt(unitDef, probePos, facing)) {											\
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
		} break;
		case UNIT_FACING_EAST: {
			DECLARE_TEST_LOW(isOpenEast, GetTypeEast);
			DO_TEST_LOW(isOpenEast);
		} break;
		case UNIT_FACING_NORTH: {
			DECLARE_TEST_LOW(isOpenNorth, GetTypeNorth);
			DO_TEST_LOW(isOpenNorth);
		} break;
		case UNIT_FACING_WEST: {
			DECLARE_TEST_LOW(isOpenWest, GetTypeWest);
			DO_TEST_LOW(isOpenWest);
		} break;
	}
#undef DO_TEST_LOW
#undef DECLARE_TEST_LOW

	return -RgtVector;
}

void CTerrainManager::MarkBlockerByMask(const SStructure& building, bool block, IBlockMask* mask)
{
	UnitDef* unitDef = building.cdef->GetDef();
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
		} break;
		case UNIT_FACING_EAST:
		case UNIT_FACING_WEST: {
			xmsize = mask->GetZSize();
			zmsize = mask->GetXSize();
			xssize = unitDef->GetZSize() / 2;
			zssize = unitDef->GetXSize() / 2;
		} break;
	}

#define DECLARE_MARKER(typeName, blockerOp, structOp)					\
	for (int z = m1.y, zm = om.y; z < m2.y; z++, zm++) {				\
		for (int x = m1.x, xm = om.x; x < m2.x; x++, xm++) {			\
			switch (mask->typeName(xm, zm)) {							\
				case IBlockMask::BlockType::BLOCK: {					\
					blockingMap.blockerOp(x, z, structType);			\
				} break;												\
				case IBlockMask::BlockType::STRUCT: {					\
					blockingMap.structOp(x, z, structType, notIgnore);	\
				} break;												\
				case IBlockMask::BlockType::OPEN: {						\
				} break;												\
			}															\
		}																\
	}

	int2 corner;
	corner.x = int(pos.x + 0.5f) / (SQUARE_SIZE * 2) - (xssize / 2);
	corner.y = int(pos.z + 0.5f) / (SQUARE_SIZE * 2) - (zssize / 2);

	int2 m1 = corner - mask->GetStructOffset(facing);	// top-left mask corner
	int2 m2(m1.x + xmsize, m1.y + zmsize);				// bottom-right mask corner
	int2 om = m1;										// store original mask corner
	blockingMap.Bound(m1, m2);							// corners bounded by map
	om = m1 - om;										// shift original mask corner

	const int notIgnore = ~mask->GetIgnoreMask();
	SBlockingMap::StructType structType = mask->GetStructType();

#define DO_MARK(facingType)												\
	if (block) {														\
		DECLARE_MARKER(facingType, AddBlocker, AddStruct);				\
	} else {															\
		DECLARE_MARKER(facingType, DelBlocker, DelStruct);				\
	}

	switch (facing) {
		default:
		case UNIT_FACING_SOUTH: {
			DO_MARK(GetTypeSouth);
		} break;
		case UNIT_FACING_EAST: {
			DO_MARK(GetTypeEast);
		} break;
		case UNIT_FACING_NORTH: {
			DO_MARK(GetTypeNorth);
		} break;
		case UNIT_FACING_WEST: {
			DO_MARK(GetTypeWest);
		} break;
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

	UnitDef* unitDef = cdef->GetDef();
	const int xsize = (((facing & 1) == 0) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;
	const int zsize = (((facing & 1) == 1) ? unitDef->GetXSize() : unitDef->GetZSize()) / 2;

	const int x1 = int(pos.x + 0.5f) / (SQUARE_SIZE * 2) - (xsize / 2), x2 = x1 + xsize;
	const int z1 = int(pos.z + 0.5f) / (SQUARE_SIZE * 2) - (zsize / 2), z2 = z1 + zsize;

	int2 m1(x1, z1);
	int2 m2(x2, z2);
	blockingMap.Bound(m1, m2);

	const SBlockingMap::StructType structType = SBlockingMap::StructType::UNKNOWN;
	const SBlockingMap::SM notIgnore = static_cast<SBlockingMap::SM>(SBlockingMap::StructMask::ALL);

	if (block) {
		for (int x = m1.x; x < m2.x; ++x) {
			for (int z = m1.y; z < m2.y; ++z) {
				blockingMap.AddStruct(x, z, structType, notIgnore);
			}
		}
	} else {
		// NOTE: This can mess up things if unit is inside factory :/
		// SOLUTION: Do not mark movable units
		for (int x = m1.x; x < m2.x; ++x) {
			for (int z = m1.y; z < m2.y; ++z) {
				blockingMap.DelStruct(x, z, structType, notIgnore);
			}
		}
	}
}

void CTerrainManager::MarkBusPath()
{
	for (auto& kv : busQueries) {
		FactoryPathQuery& fpq = kv.second;
		std::shared_ptr<IPathQuery>& pQuery = fpq.query;
		if (pQuery != nullptr) {
			continue;
		}
		pQuery = circuit->GetPathfinder()->CreatePathWideQuery(kv.first, fpq.mobileDef, fpq.startPos, fpq.endPos, fpq.targets);

		circuit->GetPathfinder()->RunQuery(circuit->GetScheduler().get(), pQuery, [this](const IPathQuery* query) {
			auto it = busPath.find(query->GetUnit());
			if (it == busPath.end()) {
				return;
			}
			const CQueryPathWide* q = static_cast<const CQueryPathWide*>(query);
			FillParentBusNodes(q->GetPathInfo().get());
			CPathFinder* pathfinder = circuit->GetPathfinder();
			const int granularity = pathfinder->GetSquareSize() / (SQUARE_SIZE * 2);
			const int howWide = pathfinder->GetSquareSize() / 32;
			for (int index : q->GetPathInfo()->path) {
				int ix, iz;
				pathfinder->PathIndex2PathXY(index, &ix, &iz);

				ix = ix * granularity + granularity / 2;
				iz = iz * granularity + granularity / 2;
				int2 m1 = (howWide & 1) ? int2(ix - 0, iz - 0) : int2(ix - 3, iz - 3);
				int2 m2 = (howWide & 1) ? int2(ix + 6, iz + 6) : int2(ix + 3, iz + 3);
				blockingMap.Bound(m1, m2);
				for (int z = m1.y; z < m2.y; ++z) {
					for (int x = m1.x; x < m2.x; ++x) {
						blockingMap.AddBlocker(x, z, SBlockingMap::StructType::TERRA);
					}
				}
			}
			it->second = q->GetPathInfo();
			busQueries.erase(query->GetUnit());
		});
	}
}

void CTerrainManager::FillParentBusNodes(CPathInfo* pathInfo)
{
	for (const auto& kv : busPath) {
		if (kv.second == nullptr) {
			continue;
		}
		const IndexVec& path = kv.second->path;
		auto it = path.begin();
		while ((it != path.end()) && (pathInfo->path.back() != *it)) {
			++it;
		}
		if ((it != path.end()) && (++it != path.end())) {
			pathInfo->path.insert(pathInfo->path.end(), it, path.end());
			return;
		}
	}
}

void CTerrainManager::SnapPosition(AIFloat3& position)
{
	// NOTE: Build-cells have size of (SQURE_SIZE * 2)=16 elmos.
	//       Build-position in engine is footprint's center, and depends on (size/2)'s oddity.
	//       With !(size & 2) => !(size/2 & 1) engine treats [0..8) as cell_0, [8..24) as cell_1.
	//       But CircuitAI treats [0..16) as cell_0, [16..32) as cell_1.
	//       Hence snap source position to multiples of (SQUARE_SIZE * 2) to avoid further oddity hussle.
	// @see rts/Sim/Units/CommandAI/BuilderCAI.cpp:CBuilderCAI::ExecuteBuildCmd()
	// @see rts/Game/GameHelper.cpp:CGameHelper::Pos2BuildPos()
	position.x = int(position.x / (SQUARE_SIZE * 2)) * SQUARE_SIZE * 2;
	position.z = int(position.z / (SQUARE_SIZE * 2)) * SQUARE_SIZE * 2;
}

std::pair<SArea*, bool> CTerrainManager::GetCurrentMapArea(CCircuitDef* cdef, const AIFloat3& position)
{
	SMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	if (mobileType == nullptr) {  // flying units & buildings
		return std::make_pair(nullptr, true);
	}

	// other mobile units & their factories
	AIFloat3 pos = position;
//	CorrectPosition(pos);
	const int iS = GetSectorIndex(pos);

	SArea* area = mobileType->sector[iS].area;
	if (area == nullptr) {
		// Case: 1) unit spawned/pushed/transported outside of valid area
		//       2) factory terraformed height around and became non-valid area
		SAreaSector* sector = GetAlternativeSector(nullptr, iS, mobileType);
		if (sector != nullptr) {
			area = sector->area;
		}
	}
	return std::make_pair(area, area != nullptr);
}

std::pair<SArea*, bool> CTerrainManager::GetCurrentMapArea(CCircuitDef* cdef, const int iS)
{
	SMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	if (mobileType == nullptr) {  // flying units & buildings
		return std::make_pair(nullptr, true);
	}

	// other mobile units & their factories
	SArea* area = mobileType->sector[iS].area;
	if (area == nullptr) {
		// Case: 1) unit spawned/pushed/transported outside of valid area
		//       2) factory terraformed height around and became non-valid area
		SAreaSector* sector = GetAlternativeSector(nullptr, iS, mobileType);
		if (sector != nullptr) {
			area = sector->area;
		}
	}
	return std::make_pair(area, area != nullptr);
}

bool CTerrainManager::CanMoveToPos(SArea* area, const AIFloat3& destination)
{
	const int iS = GetSectorIndex(destination);
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
//	CorrectPosition(pos);
	const int iS = GetSectorIndex(pos);

	SMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	SImmobileType* immobileType = GetImmobileTypeById(cdef->GetImmobileId());
	if (mobileType != nullptr) {  // a factory or mobile unit
		SAreaSector* AS = GetAlternativeSector(nullptr, iS, mobileType);
		if (immobileType != nullptr) {  // a factory
			SSector* sector = GetAlternativeSector(AS->area, iS, immobileType);
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

AIFloat3 CTerrainManager::GetMovePosition(SArea* sourceArea, const AIFloat3& position)
{
	AIFloat3 pos = position;
//	CorrectPosition(pos);
	const int iS = GetSectorIndex(pos);

	return (sourceArea == nullptr) ? pos : GetClosestSector(sourceArea, iS)->S->position;
}

std::vector<SAreaSector>& CTerrainManager::GetSectorList(SArea* sourceArea)
{
	if ((sourceArea == nullptr) || (sourceArea->mobileType == nullptr)) {  // It flies or it's immobile
		return areaData->sectorAirType;
	}
	return sourceArea->mobileType->sector;
}

SAreaSector* CTerrainManager::GetClosestSectorWithAltitude(SArea* sourceArea, const int destinationSIndex, const int altitude)
{
	std::vector<SAreaSector>& TMSectors = GetSectorList(sourceArea);
	if (sourceArea == TMSectors[destinationSIndex].area) {
		return &TMSectors[destinationSIndex];
	}

	const AIFloat3& destination = TMSectors[destinationSIndex].S->position;
	SAreaSector* SClosest = nullptr;
	float sqDisClosest = std::numeric_limits<float>::max();
	for (auto& iS : sourceArea->sector) {
		float sqDist = iS.second->S->position.SqDistance2D(destination);
		if ((sqDist < sqDisClosest) && (terrainData->GetTASector(iS.first).GetMinAltitude() >= altitude)) {
			SClosest = iS.second;
			sqDisClosest = sqDist;
		}
	}
	return SClosest;
}

SAreaSector* CTerrainManager::GetClosestSector(SArea* sourceArea, const int destinationSIndex)
{
	auto iAS = sourceArea->sectorClosest.find(destinationSIndex);
	if (iAS != sourceArea->sectorClosest.end()) {  // It's already been determined
		return iAS->second;
	}

	std::vector<SAreaSector>& TMSectors = GetSectorList(sourceArea);
	if (sourceArea == TMSectors[destinationSIndex].area) {
		sourceArea->sectorClosest[destinationSIndex] = &TMSectors[destinationSIndex];
		return &TMSectors[destinationSIndex];
	}

	const AIFloat3& destination = TMSectors[destinationSIndex].S->position;
	SAreaSector* SClosest = nullptr;
	float sqDisClosest = std::numeric_limits<float>::max();
	for (auto& iS : sourceArea->sector) {
		float sqDist = iS.second->S->position.SqDistance2D(destination);  // TODO: Consider SqDistance() instead of 2D
		if (sqDist < sqDisClosest) {
			SClosest = iS.second;
			sqDisClosest = sqDist;
		}
	}
	sourceArea->sectorClosest[destinationSIndex] = SClosest;
	return SClosest;
}

SSector* CTerrainManager::GetClosestSector(SImmobileType* sourceIT, const int destinationSIndex)
{
	auto iS = sourceIT->sectorClosest.find(destinationSIndex);
	if (iS != sourceIT->sectorClosest.end()) {  // It's already been determined
		return iS->second;
	}

	if (sourceIT->sector.find(destinationSIndex) != sourceIT->sector.end()) {
		SSector* SClosest = &areaData->sector[destinationSIndex];
		sourceIT->sectorClosest[destinationSIndex] = SClosest;
		return SClosest;
	}

	const AIFloat3& destination = areaData->sector[destinationSIndex].position;
	SSector* SClosest = nullptr;
	float sqDisClosest = std::numeric_limits<float>::max();
	for (auto& iS : sourceIT->sector) {
		float sqDist = iS.second->position.SqDistance2D(destination);  // TODO: Consider SqDistance() instead of 2D
		if (sqDist < sqDisClosest) {
			SClosest = iS.second;
			sqDisClosest = sqDist;
		}
	}
	sourceIT->sectorClosest[destinationSIndex] = SClosest;
	return SClosest;
}

SAreaSector* CTerrainManager::GetAlternativeSector(SArea* sourceArea, const int sourceSIndex, SMobileType* destinationMT)
{
	std::vector<SAreaSector>& TMSectors = GetSectorList(sourceArea);
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
	SAreaSector* bestAS = nullptr;
	SArea* largestArea = destinationMT->areaLargest;
	float bestDistance = -1.0;
	float bestMidDistance = -1.0;
	const std::vector<SArea>& TMAreas = destinationMT->area;
	for (auto& area : TMAreas) {
		if (area.areaUsable || !largestArea->areaUsable) {
			SAreaSector* CAS = GetClosestSector(const_cast<SArea*>(&area), sourceSIndex);
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
				if ((bestAS == nullptr) || (distance * area.percentOfMap < bestDistance * bestAS->area->percentOfMap)) {
					bestAS = CAS;
					bestDistance = distance;
				}
			}
		}
	}

	TMSectors[sourceSIndex].sectorAlternativeM[destinationMT] = bestAS;
	return bestAS;
}

SSector* CTerrainManager::GetAlternativeSector(SArea* destinationArea, const int sourceSIndex, SImmobileType* destinationIT)
{
	std::vector<SAreaSector>& TMSectors = GetSectorList(destinationArea);
	auto iMS = TMSectors[sourceSIndex].sectorAlternativeI.find(destinationIT);
	if (iMS != TMSectors[sourceSIndex].sectorAlternativeI.end()) {  // It's already been determined
		return iMS->second;
	}

	SSector* closestS = nullptr;
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

// NOTE: Slow after terra-update with many calls in single frame
bool CTerrainManager::CanBeBuiltAt(CCircuitDef* cdef, const AIFloat3& position, const float range)
{
	const int iS = GetSectorIndex(position);
	SSector* sector;
	SMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	SImmobileType* immobileType = GetImmobileTypeById(cdef->GetImmobileId());
	if (mobileType != nullptr) {  // a factory or mobile unit
		SAreaSector* AS = GetAlternativeSector(nullptr, iS, mobileType);
		if (AS == nullptr) {
			return false;  // FIXME: do not use units with typeUsable=false
		}
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
		if (sector == nullptr) {
			return false;  // FIXME: do not use buildings with typeUsable=false
		}
	} else {
		return true;  // flying units
	}

	if (sector == &GetSector(iS)) {  // the current sector is the best sector
		return true;
	}
	return sector->position.distance2D(GetSector(iS).position) < range;
}

bool CTerrainManager::CanBeBuiltAt(CCircuitDef* cdef, const AIFloat3& position)
{
	const int iS = GetSectorIndex(position);
	SMobileType* mobileType = GetMobileTypeById(cdef->GetMobileId());
	SImmobileType* immobileType = GetImmobileTypeById(cdef->GetImmobileId());
	if (mobileType != nullptr) {  // a factory or mobile unit
		if (mobileType->sector[iS].S == nullptr) {
			return false;
		}
		if (immobileType != nullptr) {  // a factory
			if (immobileType->sector.find(iS) == immobileType->sector.end()) {
				return false;
			}
		}
	} else if (immobileType != nullptr) {  // buildings
		if (immobileType->sector.find(iS) == immobileType->sector.end()) {
			return false;
		}
	}
	return true;
}

bool CTerrainManager::CanBeBuiltAtSafe(CCircuitDef* cdef, const AIFloat3& position)
{
	if (circuit->GetThreatMap()->GetBuilderThreatAt(position) > THREAT_MIN) {
		return false;
	}
	return CanBeBuiltAt(cdef, position);
}

bool CTerrainManager::CanReachAt(CCircuitUnit* unit, const AIFloat3& destination, const float range)
{
	if (unit->GetCircuitDef()->GetImmobileId() != -1) {  // A hub or factory
		return unit->GetPos(circuit->GetLastFrame()).SqDistance2D(destination) < SQUARE(range);
	}
	SArea* area = unit->GetArea();
	if (area == nullptr) {  // A flying unit
		return true;
	}
	const int iS = GetSectorIndex(destination);
	if (area->sector.find(iS) != area->sector.end()) {
		return true;
	}
	return GetClosestSector(area, iS)->S->position.SqDistance2D(destination) < SQUARE(range);
}

bool CTerrainManager::CanReachAtSafe(CCircuitUnit* unit, const AIFloat3& destination, const float range, const float threat)
{
	if (circuit->GetThreatMap()->GetThreatAt(destination) > threat) {
		return false;
	}
	return CanReachAt(unit, destination, range);
}

bool CTerrainManager::CanReachAtSafe2(CCircuitUnit* unit, const AIFloat3& destination, const float range)
{
	if (circuit->GetInflMap()->GetInfluenceAt(destination) < -INFL_EPS) {
		return false;
	}
	return CanReachAt(unit, destination, range);
}

bool CTerrainManager::CanMobileReachAt(SArea* area, const AIFloat3& destination, const float range)
{
	if (area == nullptr) {  // A flying unit
		return true;
	}
	const int iS = GetSectorIndex(destination);
	if (area->sector.find(iS) != area->sector.end()) {
		return true;
	}
	return GetClosestSector(area, iS)->S->position.SqDistance2D(destination) < SQUARE(range);
}

bool CTerrainManager::CanMobileReachAtSafe(SArea* area, const AIFloat3& destination, const float range, const float threat)
{
	if (circuit->GetThreatMap()->GetBuilderThreatAt(destination) > threat) {
		return false;
	}
	return CanMobileReachAt(area, destination, range);
}

const bwem::CArea* CTerrainManager::GetTAArea(const springai::AIFloat3& pos) const
{
	int iS = terrainData->GetSectorIndex(pos);
	int id = terrainData->GetTASector(iS).GetAreaId();
	if (id <= 0) {
		return nullptr;
	}
	return const_cast<const CTerrainData*>(terrainData)->GetArea(id);
}

void CTerrainManager::UpdateAreaUsers(int interval)
{
	SCOPED_TIME(circuit, __PRETTY_FUNCTION__);

	areaData = terrainData->GetNextAreaData();
	const int frame = circuit->GetLastFrame();
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;

		// Similar to GetCurrentMapArea
		SArea* area = nullptr;  // flying units & buildings
		SMobileType* mobileType = GetMobileTypeById(unit->GetCircuitDef()->GetMobileId());
		if (mobileType != nullptr) {
			// other mobile units & their factories
			AIFloat3 pos = unit->GetPos(frame);
//			CorrectPosition(pos);
			const int iS = GetSectorIndex(pos);

			area = mobileType->sector[iS].area;
			if (area == nullptr) {  // unit outside of valid area
				// TODO: Rescue operation
				SAreaSector* sector = GetAlternativeSector(nullptr, iS, mobileType);
				if (sector != nullptr) {
					area = sector->area;
				} else {
					circuit->Garbage(unit, "helpless");
				}
			}
		}
		unit->SetArea(area);
	}

	circuit->GetEnemyManager()->UpdateAreaUsers(circuit);  // AllyTeam
	enemyAreas = circuit->GetEnemyManager()->GetEnemyAreas();

	circuit->GetBuilderManager()->UpdateAreaUsers();

	// stagger area update
	circuit->GetScheduler()->RunJobAfter(CScheduler::GameJob([this]() {
		circuit->GetPathfinder()->UpdateAreaUsers(this);
		MarkBusPath();

		OnAreaUsersUpdated();
	}), interval);
}

#ifdef DEBUG_VIS
void CTerrainManager::UpdateVis()
{
	if (isWidgetDrawing) {
		std::ostringstream cmd;
		cmd << "ai_blk_data:";
		for (int z = 0; z < blockingMap.rows; ++z) {
			for (int x = 0; x < blockingMap.columns; ++x) {
				const char value = blockingMap.IsBlocked(x, z, STRUCT_BIT(ALL));
				cmd.write(&value, 1);
			}
		}
		std::string s = cmd.str();
		circuit->GetLua()->CallRules(s.c_str(), s.size());
	}

	if (dbgTextureId < 0) {
		return;
	}

	for (unsigned i = 0; i < blockingMap.gridLow.size(); ++i) {
		dbgMap[i] = (blockingMap.gridLow[i].blockerMask > 0) ? 1.0f : 0.0f;
	}
	circuit->GetDebugDrawer()->UpdateOverlayTexture(dbgTextureId, dbgMap, 0, 0, blockingMap.columnsLow, blockingMap.rowsLow);
	circuit->GetDebugDrawer()->DrawMap(sdlWindowId, dbgMap, {220, 220, 0, 0});
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
		for (unsigned i = 0; i < blockingMap.gridLow.size(); ++i) {
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

void CTerrainManager::ToggleWidgetDraw()
{
	std::string cmd("ai_thr_draw:");
	std::string result = circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

	isWidgetDrawing = (result == "1");
	if (isWidgetDrawing) {
		cmd = utils::int_to_string(16, "ai_thr_size:%i");
		cmd += utils::float_to_string(0, " %f");
		circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

		UpdateVis();
	}
}
#endif

} // namespace circuit
