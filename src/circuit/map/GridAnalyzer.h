/*
 * GridAnalyzer.h
 *
 *  Created on: Feb 9, 2022
 *      Author: rlcevg
 */

// ____________________________________________________________________________
//
// Origin: BWEM 1.4.1 - Brood War Easy Map is a C++ library that analyses
//         Brood War's maps and provides relevant information such as areas,
//         choke points and base locations.
// ____________________________________________________________________________

// (MIT/X11 License)
// Copyright (c) 2015, 2017, Igor Dimitrijevic
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
// - The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// - The Software is provided "as is", without warranty of any kind, express
// or implied, including but not limited to the warranties of merchantability,
// fitness for a particular purpose and noninfringement. In no event shall the
// authors or copyright holders be liable for any claim, damages or
// other liability, whether in an action of contract, tort or otherwise,
// arising from, out of or in connection with the software or the use or other
// dealings in the Software.
// - Except as contained in this notice, the name of the copyright holders
// shall not be used in advertising or otherwise to promote the sale, use or
// other dealings in this Software without prior written authorization from
// the copyright holders.

#ifndef SRC_CIRCUIT_MAP_GRIDANALYZER_H_
#define SRC_CIRCUIT_MAP_GRIDANALYZER_H_

#include "util/Defines.h"
#include "util/math/RagMatrix.h"
#include "util/Data.h"

#include "System/type2.h"

//#undef NDEBUG
#include <cassert>
#include <map>
#include <deque>
#include <string>
#ifdef DEBUG_VIS
#include <memory>
#endif

namespace springai {
	class AIFloat3;
#ifdef DEBUG_VIS
	class Lua;
#endif
}

namespace circuit {
	class CCircuitAI;
#ifdef DEBUG_VIS
	class CDebugDrawer;
#endif
}

namespace bwem {

#define ALTITUDE_SCALE	8

class CSector;
class CTile;
class CArea;
class CTempAreaInfo;
class CGridAnalyzer;
class IGrid;
#ifdef DEBUG_VIS
struct DebugDrawData;
#endif

using Position = springai::AIFloat3;  // full resolution
using TilePosition = int2;  // slopeMap's resolution
using SectorPosition = int2;  // AreaData sector's resolution

class CChokePoint final {
public:
	using Id = int;
	// ChokePoint::middle denotes the "middle" Tile of Geometry(), while
	// ChokePoint::end1 and ChokePoint::end2 denote its "ends".
	// It is guaranteed that, among all the Tiles of Geometry(), ChokePoint::middle has the highest altitude value.
	enum node {end1, middle, end2, node_count};

	CChokePoint(const CGridAnalyzer& ta, Id idx, const CArea* area1, const CArea* area2, const std::deque<TilePosition>& geometry);
	CChokePoint& operator=(const CChokePoint&) = delete;
	~CChokePoint() {}

	// Returns the two Areas of this ChokePoint.
	const std::pair<const CArea*, const CArea*>& GetAreas() const { return areas; }

	// Returns the center of this ChokePoint.
	const springai::AIFloat3& GetCenter() const { return GetPos(middle); }
	const springai::AIFloat3& GetEnd1() const { return GetPos(end1); }
	const springai::AIFloat3& GetEnd2() const { return GetPos(end2); }

	bool IsSmall() const { return size < 300.f; }

private:
	// Returns the position of one of the 3 nodes of this ChokePoint (Cf. node definition).
	// Note: the returned value is contained in Geometry()
	const springai::AIFloat3& GetPos(node n) const { assert(n < node_count); return nodes[n]; }

	const Id id;
	const std::pair<const CArea*, const CArea*> areas;
	springai::AIFloat3 nodes[node_count];
	float size;  // distance from end1 to end2
};

class CArea final: public utils::Markable<CArea, int> {
public:
	using Id = int;
	using Altitude = int;

	CArea(Id areaId, SectorPosition sTop, const CTile& topTile, int tilesCount);
	CArea& operator=(const CArea&) = delete;
	~CArea() {}

	// Unique id > 0 of this Area. Range = 1 .. IGrid::Areas().size()
	// this == IGrid::GetArea(Id())
	// Id() == CTerrainAnalyzer::GetTile(w).AreaId() for each walkable Tile w in this Area.
	// Area::ids are guaranteed to remain unchanged.
	Id GetId() const { return id; }

	// Unique id > 0 of the group of Areas which are accessible from this Area.
	// For each pair (a, b) of Areas: a->GroupId() == b->GroupId()  <==>  a->AccessibleFrom(b)
	// A groupId uniquely identifies a maximum set of mutually accessible Areas, that is, in the absence of blocking ChokePoints, a continent.
	Id GetGroupId() const { return groupId; }

	// Bounding box of this Area.
	const SectorPosition& GetTopLeft() const { return topLeft; }
	const SectorPosition& GetBottomRight() const { return bottomRight; }
	SectorPosition BoundingBoxSize() const { return bottomRight - topLeft + SectorPosition(1, 1); }

	// Returns CTerrainAnalyzer::GetTile(Top()).Altitude().
	CArea::Altitude GetMaxAltitude() const { return maxAltitude; }

	// Position of the Sector with the highest Tile Altitude() value.
	const SectorPosition& GetTop() const { return top; }

	// Returns the number of MiniTiles in this Area.
	// This most accurately defines the size of this Area.
	int GetNumTiles() const { return numTiles; }
	int GetNumSectors() const { return numSectors; }
	int GetNumSmallChokes() const { return numSmallChokes; }

	// Returns the ChokePoints between this Area and the neighbouring ones.
	// Note: if there are no neighbouring Areas, then an empty set is returned.
	// Note there may be more ChokePoints returned than the number of neighbouring Areas, as there may be several ChokePoints between two Areas (Cf. ChokePoints(const Area * pArea)).
	const std::vector<const CChokePoint*> & GetChokePoints() const { return chokePoints; }

	// Returns the ChokePoints between this Area and pArea.
	// Assumes pArea is a neighbour of this Area, i.e. ChokePointsByArea().find(pArea) != ChokePointsByArea().end()
	// Note: there is always at least one ChokePoint between two neighbouring Areas.
	const std::vector<CChokePoint>& GetChokePoints(const CArea* pArea) const;

	// Returns the ChokePoints of this Area grouped by neighbouring Areas
	// Note: if there are no neighbouring Areas, than an empty set is returned.
	const std::map<const CArea*, const std::vector<CChokePoint>*>& GetChokePointsByArea() const { return chokePointsByArea; }

	// Returns the accessible neighbouring Areas.
	// The accessible neighbouring Areas are a subset of the neighbouring Areas (the neighbouring Areas can be iterated using ChokePointsByArea()).
	// Two neighbouring Areas are accessible from each over if at least one the ChokePoints they share is not Blocked (Cf. ChokePoint::Blocked).
	const std::vector<const CArea*>& GetAccessibleNeighbours() const { return accessibleNeighbours; }

	// Returns whether this Area is accessible from pArea, that is, if they share the same GroupId().
	// Note: accessibility is always symmetrical.
	// Note: even if a and b are neighbouring Areas,
	//       we can have: a->AccessibleFrom(b)
	//       and not:     contains(a->AccessibleNeighbours(), b)
	// See also GroupId()
	bool AccessibleFrom(const CArea* pArea) const { return GetGroupId() == pArea->GetGroupId(); }

	////////////////////////////////////////////////////////////////////////////
	//	Details: The functions below are used by the BWEM's internals
	void AddChokePoints(CArea* pArea, std::vector<CChokePoint>* pChokePoints);
	void AddSectorInformation(const SectorPosition t, const CSector& sector);
	void UpdateAccessibleNeighbours();
	void SetGroupId(Id gid) { assert(gid >= 1); groupId = gid; }

private:
	Id id;
	Id groupId = 0;
	SectorPosition top;  // NOTE: BWEM has TilePosition, but here tiles are not saved
	SectorPosition topLeft     = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
	SectorPosition bottomRight = {std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
	Altitude maxAltitude;
	int numTiles;
	int numSectors = 0;

	std::map<const CArea*, const std::vector<CChokePoint>*> chokePointsByArea;
	std::vector<const CArea*> accessibleNeighbours;
	std::vector<const CChokePoint*> chokePoints;
	int numSmallChokes = 0;
};

class CTile final {
public:
	CTile() {}
	CTile& operator=(const CTile&) = delete;
	~CTile() {}

	//  - For each unwalkable Tile, we also mark its 8 neighbours as not walkable.
	//    According to some tests, this prevents from wrongly pretending one small unit can go by some thin path.
	// Among the Tiles having Altitude() > 0, the walkable ones are considered Terrain-Tiles, and the other ones Lake-Tiles.
	bool IsWalkable() const { return areaId != 0; }

	// Distance in pixels between the center of this Tile and the center of the nearest Sea-Tile
	// Sea-Tiles all have their Altitude() equal to 0.
	// Tiles having Altitude() > 0 are not Sea-Tiles. They can be either Terrain-Tiles or Lake-Tiles.
	CArea::Altitude GetAltitude() const { return altitude; }

	// Sea-Tiles are unwalkable Tiles that have their Altitude() equal to 0.
	bool IsSea() const { return altitude == 0; }

	// For Sea and Lake Tiles, returns 0
	// For Terrain Tiles, returns a non zero id:
	//    - if (id > 0), id uniquely identifies the Area A that contains this Tile.
	//    - if (id < 0), then this Tile is part of a Terrain-zone that was considered too small to create an Area for it.
	//      Note: negative Area::ids start from -2
	CArea::Id GetAreaId() const { return areaId; }

	TilePosition GetSource() const { return source; }

	////////////////////////////////////////////////////////////////////////////
	//	Details: The functions below are used by the BWEM's internals
	void SetWalkable(bool walkable) { areaId = (walkable ? -1 : 0); altitude = (walkable ? -1 : 1); }
	bool IsSeaOrLake() const { return altitude == 1; }
	void SetSea() { assert(!IsWalkable() && IsSeaOrLake()); altitude = 0; }
	void SetLake() { assert(!IsWalkable() && IsSea()); altitude = -1; }
	bool IsAltitudeMissing() const { return altitude == -1; }
	void SetAltitude(CArea::Altitude a, TilePosition origin) { assert(IsAltitudeMissing() && (a > 0)); altitude = a; source = origin; }
	bool IsAreaIdMissing() const { return areaId == -1; }
	void SetAreaId(CArea::Id id) { assert(IsAreaIdMissing() && (id >= 1)); areaId = id; }
	void ReplaceAreaId(CArea::Id id) { assert((areaId > 0) && ((id >= 1) || (id <= -2)) && (id != areaId)); areaId = id; }

private:
	CArea::Altitude altitude = -1;  // 0 for seas;  != 0 for terrain and lakes (-1 = not computed yet);  1 = SeaOrLake intermediate value
	CArea::Id       areaId   = -1;  // 0 -> unwalkable;  > 0 -> index of some Area;  < 0 -> some walkable terrain, but too small to be part of an Area
	TilePosition    source;
};

class CSector {  // NxN CTile
public:
	// Tile::AreaId() somewhat aggregates the MiniTile::AreaId() values of the 4 x 4 sub-MiniTiles.
	// Let S be the set of MiniTile::AreaId() values for each walkable MiniTile in this Tile.
	// If empty(S), returns 0. Note: in this case, no contained MiniTile is walkable, so all of them have their AreaId() == 0.
	// If S = {a}, returns a (whether positive or negative).
	// If size(S) > 1 returns -1 (note that -1 is never returned by MiniTile::AreaId()).
	CArea::Id GetAreaId() const { return areaId; }

	// Tile::MinAltitude() somewhat aggregates the MiniTile::Altitude() values of the 4 x 4 sub-MiniTiles.
	// Returns the minimum value.
	CArea::Altitude GetMinAltitude() const { return minAltitude; }

	void SetAreaId(CArea::Id id) { assert((id == -1) || (!areaId && id)); areaId = id; }
	void SetMinAltitude(CArea::Altitude a) { assert(a >= 0); minAltitude = a; }

private:
	CArea::Altitude minAltitude;
	CArea::Id areaId = 0;
};

class IGrid {
protected:
	IGrid();
	IGrid(const IGrid&) = delete;
	IGrid& operator=(const IGrid&) = delete;
public:
	virtual ~IGrid();

	virtual bool IsWalkable(int xSlope, int ySlope) const = 0;

	const CArea* GetArea(CArea::Id id) const { assert(IsValid(id)); return &areas[id - 1]; }
	const std::vector<CChokePoint*>& GetChokePoints() const { return chokePointList; }
	const std::vector<CChokePoint>& GetChokePoints(CArea::Id a, CArea::Id b) const;
	const std::vector<CChokePoint>& GetChokePoints(const CArea* a, const CArea* b) const {
		return GetChokePoints(a->GetId(), b->GetId());
	}
	const CSector& GetSector(const SectorPosition p) const { return sectors[sectorXSize * p.y + p.x]; }
	const CSector& GetTASector(const int index) const { return sectors[index]; }
	const std::vector<CArea>& GetAreas() const { return areas; }

protected:
	friend class CGridAnalyzer;

	CArea* GetArea(CArea::Id id) { assert(IsValid(id)); return &areas[id - 1]; }
	std::vector<CChokePoint>& GetChokePoints(CArea::Id a, CArea::Id b) {
		return const_cast<std::vector<CChokePoint>&>(static_cast<const IGrid&>(*this).GetChokePoints(a, b));
	}
	std::vector<CChokePoint>& GetChokePoints(const CArea* a, const CArea* b) {
		return GetChokePoints(a->GetId(), b->GetId());
	}
	CSector& GetSector(const SectorPosition p) { return const_cast<CSector&>(static_cast<const IGrid&>(*this).GetSector(p)); }
	std::vector<CArea>& GetAreas() { return areas; }
	int AreasCount() const { return (int)areas.size(); }
	bool IsValid(CArea::Id id) const { return (1 <= id) && (id <= AreasCount()); }

	void CreateAreas(const CGridAnalyzer& ta, const std::vector<std::pair<TilePosition, int>>& areasList);
	void CreateChokePoints(const CGridAnalyzer& ta);
	void SetAreaIdInSectors(const CGridAnalyzer& ta);
	void CollectInformation();
	void UpdateGroupIds();

	std::vector<CArea> areas;
	std::vector<CChokePoint*> chokePointList;
	circuit::CRagMatrix<std::vector<CChokePoint>> chokePointsMatrix;  // index == Area::id x Area::id

	circuit::CRagMatrix<std::vector<SectorPosition>> veinsMatrix;

	std::vector<CSector> sectors;
public:
	int sectorXSize;
	int sectorZSize;

#ifdef DEBUG_VIS
public:
	std::shared_ptr<circuit::CDebugDrawer> debugDrawer;
	springai::Lua* debugLua;
	std::pair<uint32_t, float*> tileWin;
	std::pair<uint32_t, float*> sectorWin;
	int toggleFrame;
	DebugDrawData* debugData;
	void UpdateTAVis();
	void ToggleTAVis(int frame);
#endif  // DEBUG_VIS
};

class CGridAnalyzer final {
public:
	struct SConfig {
		std::string unitName;
		int lakeMaxTiles;
		int lakeMaxWidthInTiles;
		int areaMinTiles;
		int areasMergeSize;
		int areasMergeAltitude;
		int2 tileSize;
	};
	struct SFrontier {
		CArea::Id areaId1;
		CArea::Id areaId2;
		TilePosition pos;
	};

	CGridAnalyzer(IGrid* const grid, const SConfig& cfg);
	CGridAnalyzer(const CGridAnalyzer&) = delete;
	CGridAnalyzer& operator=(const CGridAnalyzer&) = delete;
	~CGridAnalyzer();

	void Analyze(circuit::CCircuitAI* circuit);
	const std::vector<SFrontier>& GetRawFrontier() const { return rawFrontier; }

	const SConfig& GetConfig() const { return config; }
	const CTile& GetTile(const TilePosition p) const { return tiles[config.tileSize.x * p.y + p.x]; }

	springai::AIFloat3 Tile2Pos(const TilePosition p) const {
		return springai::AIFloat3(p.x * SLOPE_TILE + SLOPE_TILE / 2, 0.f, p.y * SLOPE_TILE + SLOPE_TILE / 2);
	}

private:
	CTile& GetTile(const TilePosition p) { return const_cast<CTile&>(static_cast<const CGridAnalyzer&>(*this).GetTile(p)); }
	bool IsValid(const TilePosition p) const {
		return (p.x >= 0) && (p.x < config.tileSize.x) && (p.y >= 0) && (p.y < config.tileSize.y);
	}
	bool IsSeaSide(const TilePosition p) const;

	void LoadData();
	void DecideSeasOrLakes();
	void ComputeAltitude();  // Depth of passable tile from edge of outer contour
	void ComputeAreas();
	std::vector<std::pair<TilePosition, CTile*>> SortTiles();
	std::pair<CArea::Id, CArea::Id> FindNeighboringAreas(TilePosition p) const;
	CArea::Id ChooseNeighboringArea(CArea::Id a, CArea::Id b);
	std::vector<CTempAreaInfo> ComputeTempAreas(const std::vector<std::pair<TilePosition, CTile*>>& tilesByDescendingAltitude);
	void ReplaceAreaIds(TilePosition p, CArea::Id newAreaId);
	void CreateAreas(const std::vector<CTempAreaInfo>& tempAreaList);

	IGrid* const grid;  // graph

	const SConfig config;
	std::vector<CTile> tiles;
	std::map<std::pair<CArea::Id, CArea::Id>, int> areaPairCounter;

	std::vector<SFrontier> rawFrontier;
	CArea::Altitude maxAltitude;
};

#ifdef DEBUG_VIS
struct DebugDrawData {
	int2 tileSize;
	std::vector<CTile> tiles;
	std::vector<CGridAnalyzer::SFrontier> rawFrontier;
	CArea::Altitude maxAltitude;
};
#endif  // DEBUG_VIS

} // namespace bwem

#endif // SRC_CIRCUIT_MAP_GRIDANALYZER_H_
