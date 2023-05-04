/*
 * GridAnalyzer.cpp
 *
 *  Created on: Feb 9, 2022
 *      Author: rlcevg
 */

#include "map/GridAnalyzer.h"
#include "terrain/TerrainData.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "Log.h"

namespace bwem {

using namespace circuit;
using namespace springai;
#ifdef DEBUG_VIS
springai::Lua* gDebugLua;
#endif

CChokePoint::CChokePoint(const CGridAnalyzer& ta, Id idx, const CArea* area1, const CArea* area2, const std::deque<TilePosition>& geometry)
		: id(idx), areas(area1, area2)
{
	assert(!geometry.empty());

	nodes[end1] = ta.Tile2Pos(geometry.front());
	nodes[end2] = ta.Tile2Pos(geometry.back());
	size = nodes[end1].distance2D(nodes[end2]);

	int i = geometry.size() / 2;
	// NOTE: Related to lakes
//	while ((i > 0) && (ta.GetTile(geometry[i - 1]).GetAltitude() > ta.GetTile(geometry[i]).GetAltitude())) {
//		--i;
//	}
//	while ((i < (int)geometry.size() - 1) && (ta.GetTile(geometry[i + 1]).GetAltitude() > ta.GetTile(geometry[i]).GetAltitude())) {
//		++i;
//	}
	nodes[middle] = ta.Tile2Pos(geometry[i]);
}

CArea::CArea(Id areaId, SectorPosition topSec, const CTile& topTile, int tilesCount)
		: id(areaId), top(topSec), numTiles(tilesCount)
{
	assert(areaId > 0);
	assert(topTile.GetAreaId() == id);

	maxAltitude = topTile.GetAltitude();
}

const std::vector<CChokePoint>& CArea::GetChokePoints(const CArea* pArea) const
{
	auto it = chokePointsByArea.find(pArea);
	assert(it != chokePointsByArea.end());
	return *it->second;
}

void CArea::AddChokePoints(CArea* pArea, std::vector<CChokePoint>* pChokePoints)
{
	assert(!chokePointsByArea[pArea] && pChokePoints);

	chokePointsByArea[pArea] = pChokePoints;

	for (const auto& cp : *pChokePoints) {
		chokePoints.push_back(&cp);
		if (cp.IsSmall()) {
			++numSmallChokes;
		}
	}
}

void CArea::AddSectorInformation(const SectorPosition t, const CSector& sector)
{
	++numSectors;

	if (t.x < topLeft.x)     topLeft.x = t.x;
	if (t.y < topLeft.y)     topLeft.y = t.y;
	if (t.x > bottomRight.x) bottomRight.x = t.x;
	if (t.y > bottomRight.y) bottomRight.y = t.y;
}

void CArea::UpdateAccessibleNeighbours()
{
	accessibleNeighbours.clear();

	for (auto it : GetChokePointsByArea()) {
		// NOTE: BWEM adds area only when there's at least one !chokePoint.Blocked()
		accessibleNeighbours.push_back(it.first);
	}
}

IGrid::IGrid()
		: sectorXSize(0)
		, sectorZSize(0)
#ifdef DEBUG_VIS
		, debugLua(nullptr)
		, toggleFrame(-1)
		, debugData(new DebugDrawData)
#endif
{
}

IGrid::~IGrid()
{
#ifdef DEBUG_VIS
	if (debugDrawer != nullptr && tileWin.second != nullptr) {
		debugDrawer->DelSDLWindow(tileWin.first);
		delete[] tileWin.second;
		debugDrawer->DelSDLWindow(sectorWin.first);
		delete[] sectorWin.second;
	}
	delete debugData;
#endif
}

const std::vector<CChokePoint>& IGrid::GetChokePoints(CArea::Id a, CArea::Id b) const
{
	assert(IsValid(a));
	assert(IsValid(b));
	assert(a != b);

	if (a > b) std::swap(a, b);

	return chokePointsMatrix(b, a);
}

void IGrid::CreateAreas(const CGridAnalyzer& ta, const std::vector<std::pair<TilePosition, int>>& areasList)
{
	const int convertStoSM = ta.GetConfig().tileSize.x / sectorXSize;

	areas.reserve(areasList.size());
	for (CArea::Id id = 1; id <= (CArea::Id)areasList.size(); ++id) {
		TilePosition top = areasList[id - 1].first;
		SectorPosition topSec = top / convertStoSM;
		int tilesCount = areasList[id - 1].second;
		areas.emplace_back(id, topSec, ta.GetTile(top), tilesCount);
	}
}

void IGrid::CreateChokePoints(const CGridAnalyzer& ta)
{
	auto queenWiseDist = [](TilePosition A, TilePosition B) { A -= B; return utils::queenWiseNorm(A.x, A.y); };
	CChokePoint::Id newIndex = 0;

	// 1) Size the matrix
	chokePointsMatrix.Resize(AreasCount() + 1);  // triangular matrix

	// 2) Dispatch the global raw frontier between all the relevant pairs of Areas:
	std::map<std::pair<CArea::Id, CArea::Id>, std::vector<TilePosition>> rawFrontierByAreaPair;
	for (const CGridAnalyzer::SFrontier& raw : ta.GetRawFrontier()) {
		CArea::Id a = raw.areaId1;
		CArea::Id b = raw.areaId2;
		if (a > b) std::swap(a, b);
		assert(a <= b);
		assert((a >= 1) && (b <= AreasCount()));

		rawFrontierByAreaPair[std::make_pair(a, b)].push_back(raw.pos);
	}

	// 3) For each pair of Areas (A, B):
	for (auto& raw : rawFrontierByAreaPair) {
		CArea::Id a = raw.first.first;
		CArea::Id b = raw.first.second;

		const std::vector<TilePosition>& rawFrontierAB = raw.second;
#ifndef NDEBUG
		// Because our dispatching preserved order,
		// and because Map::m_RawFrontier was populated in descending order of the altitude (see Map::ComputeAreas),
		// we know that RawFrontierAB is also ordered the same way, but let's check it:
		{
			std::vector<CArea::Altitude> altitudes;
			for (auto w : rawFrontierAB) {
				altitudes.push_back(ta.GetTile(w).GetAltitude());
			}

			assert(std::is_sorted(altitudes.rbegin(), altitudes.rend()));
		}
#endif
		// 3.1) Use that information to efficiently cluster RawFrontierAB in one or several chokepoints.
		//    Each cluster will be populated starting with the center of a chokepoint (max altitude)
		//    and finishing with the ends (min altitude).
		const int clusterMinDist = 1;  // (int)sqrt(ta.GetConfig().lakeMaxTiles);
		std::vector<std::deque<TilePosition>> clusters;
		for (auto w : rawFrontierAB) {
			bool added = false;
			for (auto& cluster : clusters) {
				int distToFront = queenWiseDist(cluster.front(), w);
				int distToBack = queenWiseDist(cluster.back(), w);
				if (std::min(distToFront, distToBack) <= clusterMinDist) {
					if (distToFront < distToBack)	cluster.push_front(w);
					else							cluster.push_back(w);

					added = true;
					break;
				}
			}

			if (!added) clusters.push_back(std::deque<TilePosition>(1, w));
		}

		// 3.2) Create one Chokepoint for each cluster:
		GetChokePoints(a, b).reserve(clusters.size());
		for (const auto& cluster : clusters) {
			GetChokePoints(a, b).emplace_back(ta, newIndex++, GetArea(a), GetArea(b), cluster);
		}
	}

	// 4) Create one Chokepoint for each pair of blocked areas, for each blocking Neutral:
	// NOTE: BlockingNeutrals were cut off

	// 5) Set the references to the freshly created Chokepoints:
	for (CArea::Id a = 1; a <= AreasCount(); ++a) {
		for (CArea::Id b = 1; b < a; ++b) {
			if (GetChokePoints(a, b).empty()) {
				continue;
			}
			GetArea(a)->AddChokePoints(GetArea(b), &GetChokePoints(a, b));
			GetArea(b)->AddChokePoints(GetArea(a), &GetChokePoints(a, b));

			for (auto& cp : GetChokePoints(a, b)) {
				chokePointList.push_back(&cp);
			}
		}
	}
}

void IGrid::SetAreaIdInSectors(const CGridAnalyzer& ta)
{
	const int convertStoSM = ta.GetConfig().tileSize.x / sectorXSize;

	auto setAreaIdInSector = [&](const SectorPosition p) {
		CSector& sector = GetSector(p);
		assert(sector.GetAreaId() == 0);  // initialized to 0

		TilePosition t = p * convertStoSM;
		for (int dy = 0; dy < convertStoSM; ++dy) {
			for (int dx = 0; dx < convertStoSM; ++dx) {
				if (CArea::Id id = ta.GetTile(t + TilePosition(dx, dy)).GetAreaId()) {
					if (sector.GetAreaId() == 0) sector.SetAreaId(id);
					else if (sector.GetAreaId() != id) {
						sector.SetAreaId(-1);
						return;
					}
				}
			}
		}
	};

	auto setAltitudeInSector = [&](const SectorPosition p) {
		CArea::Altitude minAltitude = std::numeric_limits<CArea::Altitude>::max();

		TilePosition t = p * convertStoSM;
		for (int dy = 0; dy < convertStoSM; ++dy) {
			for (int dx = 0; dx < convertStoSM; ++dx) {
				CArea::Altitude altitude = ta.GetTile(t + TilePosition(dx, dy)).GetAltitude();
				if (altitude < minAltitude) minAltitude = altitude;
			}
		}

		GetSector(p).SetMinAltitude(minAltitude);
	};

	for (int y = 0; y < sectorZSize; ++y) {
		for (int x = 0 ; x < sectorXSize; ++x) {
			const SectorPosition w(x, y);
			setAreaIdInSector(w);
			setAltitudeInSector(w);
		}
	}
}

void IGrid::CollectInformation()
{
	for (CArea& area : GetAreas()) {
		area.UpdateAccessibleNeighbours();
	}

	UpdateGroupIds();

	for (int y = 0; y < sectorZSize; ++y) {
		for (int x = 0; x < sectorXSize; ++x) {
			const CSector& sector = GetSector(TilePosition(x, y));
			if (sector.GetAreaId() > 0) {
				GetArea(sector.GetAreaId())->AddSectorInformation(TilePosition(x, y), sector);
			}
		}
	}
}

void IGrid::UpdateGroupIds()
{
	CArea::Id nextGroupId = 1;

	CArea::UnmarkAll();
	for (CArea& start : GetAreas()) {
		if (!start.Marked()) {
			std::vector<CArea*> toVisit{&start};
			while (!toVisit.empty()) {
				CArea* current = toVisit.back();
				toVisit.pop_back();
				current->SetGroupId(nextGroupId);

				for (const CArea* next : current->GetAccessibleNeighbours()) {
					if (!next->Marked()) {
						next->SetMarked();
						toVisit.push_back(const_cast<CArea*>(next));
					}
				}
			}
			++nextGroupId;
		}
	}
}

#ifdef DEBUG_VIS
void IGrid::UpdateTAVis()
{
	if ((debugDrawer == nullptr) || tileWin.second == nullptr) {
		return;
	}

	debugDrawer->DrawTex(tileWin.first, tileWin.second);
	debugDrawer->DrawTex(sectorWin.first, sectorWin.second);

	std::ostringstream cmd;
	cmd << "ai_thr_data:";
	std::vector<float> dataArray;
	dataArray.reserve(sectors.size());
	for (int i = 0; i < debugData->tileSize.x * debugData->tileSize.y; ++i) {
		dataArray.push_back(debugData->tiles[i].GetAltitude());
	}
	cmd.write(reinterpret_cast<const char*>(dataArray.data()), sectors.size() * sizeof(float));
	std::string s = cmd.str();
	debugLua->CallRules(s.c_str(), s.size());
}

void IGrid::ToggleTAVis(int frame)
{
	if ((debugDrawer == nullptr) || (toggleFrame >= frame)) {
		return;
	}
	toggleFrame = frame;

	std::string cmd("ai_thr_print:");
	debugLua->CallRules(cmd.c_str(), cmd.size());
//	std::string cmd("ai_thr_draw:");
//	debugLua->CallRules(cmd.c_str(), cmd.size());
	cmd = "ai_thr_size:16 0";
	debugLua->CallRules(cmd.c_str(), cmd.size());

	if (tileWin.second == nullptr) {
		// ~choke
		sectorWin.second = new float [sectorXSize * sectorZSize * 3];
		sectorWin.first = debugDrawer->AddSDLWindow(sectorXSize, sectorZSize, "Circuit AI :: Choke Sector");
		tileWin.second = new float [debugData->tileSize.x * debugData->tileSize.y * 3];
		tileWin.first = debugDrawer->AddSDLWindow(debugData->tileSize.x, debugData->tileSize.y, "Circuit AI :: Choke Tile");

		for (int i = 0; i < debugData->tileSize.x * debugData->tileSize.y; ++i) {
			if (debugData->tiles[i].GetAltitude() >= 0) {
				tileWin.second[i * 3 + 0] = 0.2f * debugData->tiles[i].GetAltitude() / debugData->maxAltitude;
				tileWin.second[i * 3 + 1] = 1.0f * debugData->tiles[i].GetAltitude() / debugData->maxAltitude;
				tileWin.second[i * 3 + 2] = 0.2f * debugData->tiles[i].GetAltitude() / debugData->maxAltitude;
			} else {
				tileWin.second[i * 3 + 0] = 0.4f;
				tileWin.second[i * 3 + 1] = 0.4f;
				tileWin.second[i * 3 + 2] = 0.4f;
			}
		}
		for (const CGridAnalyzer::SFrontier& f : debugData->rawFrontier) {
			int i = debugData->tileSize.x * f.pos.y + f.pos.x;
			tileWin.second[i * 3 + 0] = 0.9f;
			tileWin.second[i * 3 + 1] = 0.9f;
			tileWin.second[i * 3 + 2] = 0.2f;
		}
		for (CArea::Id a = 1; a <= AreasCount(); ++a) {
			for (CArea::Id b = 1; b < a; ++b) {
				const auto& chokes = GetChokePoints(a, b);
				for (const CChokePoint& cp : chokes) {
					TilePosition p(cp.GetCenter().x / SLOPE_TILE, cp.GetCenter().z / SLOPE_TILE);
					int i = debugData->tileSize.x * p.y + p.x;
					tileWin.second[i * 3 + 0] = 0.2f;
					tileWin.second[i * 3 + 1] = 0.2f;
					tileWin.second[i * 3 + 2] = 0.9f;
				}
			}
		}

		for (int i = 0; i < sectorXSize * sectorZSize; ++i) {
			if (sectors[i].GetMinAltitude() >= 0) {
				sectorWin.second[i * 3 + 0] = 0.2f * sectors[i].GetMinAltitude() / debugData->maxAltitude;
				sectorWin.second[i * 3 + 1] = 1.0f * sectors[i].GetMinAltitude() / debugData->maxAltitude;
				sectorWin.second[i * 3 + 2] = 0.2f * sectors[i].GetMinAltitude() / debugData->maxAltitude;
			} else {
				sectorWin.second[i * 3 + 0] = 0.4f;
				sectorWin.second[i * 3 + 1] = 0.4f;
				sectorWin.second[i * 3 + 2] = 0.4f;
			}
		}

		UpdateTAVis();

//		for (CArea::Id a = 1; a <= AreasCount(); ++a) {
//			for (CArea::Id b = 1; b < a; ++b) {
//				const auto& chokes = GetChokePoints(a, b);
//				for (const CChokePoint& cp : chokes) {
//					AIFloat3 pos = cp.GetCenter();
//					std::string cmd("ai_mrk_add:");
//					cmd += utils::int_to_string(pos.x) + " " + utils::int_to_string(pos.z) + " 16 0.2 0.2 0.9 9";
//					debugLua->CallRules(cmd.c_str(), cmd.size());
//				}
//			}
//		}
//		std::ostringstream cmd;
//		cmd << "ai_blk_data:";
//		char tmp[debugData->tileSize.x * debugData->tileSize.y] = {0};
//		for (const CGridAnalyzer::SFrontier& f : debugData->rawFrontier) {
//			tmp[debugData->tileSize.x * f.pos.y + f.pos.x] = 255;
//		}
//		cmd.write(&tmp[0], debugData->tileSize.x * debugData->tileSize.y);
//		std::string s = cmd.str();
//		debugLua->CallRules(s.c_str(), s.size());

	} else {

		std::string cmd = "ai_mrk_clear:";
		debugLua->CallRules(cmd.c_str(), cmd.size());

		debugDrawer->DelSDLWindow(tileWin.first);
		delete[] tileWin.second;
		tileWin.second = nullptr;
		debugDrawer->DelSDLWindow(sectorWin.first);
		delete[] sectorWin.second;
		sectorWin.second = nullptr;
	}
}
#endif  // DEBUG_VIS

CGridAnalyzer::CGridAnalyzer(IGrid* const grid, const SConfig& cfg)
		: grid(grid)
		, config(cfg)
		, maxAltitude(1)  // 1 for division
{
}

CGridAnalyzer::~CGridAnalyzer()
{
}

void CGridAnalyzer::Analyze(CCircuitAI* circuit)
{
#ifdef DEBUG_VIS
gDebugLua = circuit->GetLua();
#endif
	tiles.resize(config.tileSize.x * config.tileSize.y);

	LoadData();
	DecideSeasOrLakes();
	ComputeAltitude();
	ComputeAreas();
	grid->CreateChokePoints(*this);
	grid->CollectInformation();

#ifdef DEBUG_VIS
	grid->debugDrawer = circuit->GetDebugDrawer();
	grid->debugLua = circuit->GetLua();
	grid->debugData->tileSize = config.tileSize;
	grid->debugData->tiles = std::move(tiles);
	grid->debugData->rawFrontier = std::move(rawFrontier);
	grid->debugData->maxAltitude = maxAltitude;
#else  // DEBUG_VIS
	tiles.clear();
	rawFrontier.clear();
#endif  // DEBUG_VIS
	areaPairCounter.clear();
}

bool CGridAnalyzer::IsSeaSide(const TilePosition p) const
{
	if (!GetTile(p).IsSea()) {
		return false;
	}

	for (const TilePosition delta : {TilePosition(0, -1), TilePosition(-1, 0), TilePosition(+1, 0), TilePosition(0, +1)}) {
		const TilePosition np = p + delta;
		if (IsValid(np) && !GetTile(np).IsSea()) {
			return true;
		}
	}

	return false;
}

void CGridAnalyzer::LoadData()
{
	// Mark unwalkable tiles (tiles are walkable by default)
	for (int y = 0; y < config.tileSize.y; ++y) {
		for (int x = 0; x < config.tileSize.x; ++x) {
			if (grid->IsWalkable(x, y)) {
				continue;
			}
#if 1
			GetTile(TilePosition(x, y)).SetWalkable(false);
#else
			// For each unwalkable minitile, we also mark its 8 neighbours as not walkable.
			// According to some tests, this prevents from wrongly pretending one Marine can go by some thin path.
			for (int dy = -1; dy <= +1; ++dy) {
				for (int dx = -1; dx <= +1; ++dx) {
					TilePosition w(x + dx, y + dy);
					if (IsValid(w)) {
						GetTile(w).SetWalkable(false);
					}
				}
			}
#endif
		}
	}
}

void CGridAnalyzer::DecideSeasOrLakes()
{
	for (int y = 0; y < config.tileSize.y; ++y) {
		for (int x = 0; x < config.tileSize.x; ++x) {
			TilePosition origin = TilePosition(x, y);
			CTile& tOrigin = GetTile(origin);
			if (tOrigin.IsSeaOrLake()) {
				std::vector<TilePosition> toSearch{origin};
				std::vector<CTile*> seaExtent{&tOrigin};
				tOrigin.SetSea();
				TilePosition topLeft = origin;
				TilePosition bottomRight = origin;
				while (!toSearch.empty()) {
					TilePosition current = toSearch.back();
					if (current.x < topLeft.x) topLeft.x = current.x;
					if (current.y < topLeft.y) topLeft.y = current.y;
					if (current.x > bottomRight.x) bottomRight.x = current.x;
					if (current.y > bottomRight.y) bottomRight.y = current.y;

					toSearch.pop_back();
					for (TilePosition delta : {TilePosition(0, -1), TilePosition(-1, 0), TilePosition(+1, 0), TilePosition(0, +1)}) {
						TilePosition next = current + delta;
						if (IsValid(next)) {
							CTile& tNext = GetTile(next);
							if (tNext.IsSeaOrLake()) {
								toSearch.push_back(next);
								if ((int)seaExtent.size() <= config.lakeMaxTiles) seaExtent.push_back(&tNext);
								tNext.SetSea();
							}
						}
					}
				}

				if (((int)seaExtent.size() <= config.lakeMaxTiles) &&
					(bottomRight.x - topLeft.x <= config.lakeMaxWidthInTiles) &&
					(bottomRight.y - topLeft.y <= config.lakeMaxWidthInTiles) &&
					(topLeft.x >= 2) && (topLeft.y >= 2) && (bottomRight.x < config.tileSize.x - 2) && (bottomRight.y < config.tileSize.y - 2))
				{
					for (CTile* pSea : seaExtent) {
						pSea->SetLake();
					}
				}
			}
		}
	}
}

void CGridAnalyzer::ComputeAltitude()
{
	// 1) Fill in and sort DeltasByAscendingAltitude
	const int range = std::max(config.tileSize.x, config.tileSize.y) / 2 + 3;

	std::vector<std::pair<TilePosition, CArea::Altitude>> deltasByAscendingAltitude;

	for (int dy = 0; dy <= range; ++dy) {
		for (int dx = dy; dx <= range; ++dx) {  // Only consider 1/8 of possible deltas. Other ones obtained by symmetry.
			if (dx || dy) {
				deltasByAscendingAltitude.emplace_back(TilePosition(dx, dy),
						CArea::Altitude(0.5f + utils::norm(dx, dy) * ALTITUDE_SCALE));
			}
		}
	}

	std::sort(deltasByAscendingAltitude.begin(), deltasByAscendingAltitude.end(),
	[](const std::pair<TilePosition, CArea::Altitude>& a, const std::pair<TilePosition, CArea::Altitude>& b) {
		return a.second < b.second;
	});

	// 2) Fill in ActiveSeaSideList, which basically contains all the seaside miniTiles (from which altitudes are to be computed)
	//    It also includes extra border-miniTiles which are considered as seaside miniTiles too.
	struct ActiveSeaSide {
		TilePosition origin;
		CArea::Altitude lastAltitudeGenerated;
	};
	std::vector<ActiveSeaSide> activeSeaSideList;

	for (int y = -1; y <= config.tileSize.y; ++y) {
		for (int x = -1 ; x <= config.tileSize.x; ++x) {
			const TilePosition w(x, y);
			if (!IsValid(w) || IsSeaSide(w)) {
				activeSeaSideList.push_back(ActiveSeaSide{w, 0});
			}
		}
	}

	// 3) Dijkstra's algorithm
	for (const auto& delta_altitude : deltasByAscendingAltitude) {  // floodFill
		const TilePosition d = delta_altitude.first;
		const CArea::Altitude altitude = delta_altitude.second;
		for (int i = 0 ; i < (int)activeSeaSideList.size() ; ++i) {
			ActiveSeaSide& current = activeSeaSideList[i];
			if (altitude - current.lastAltitudeGenerated >= 2 * ALTITUDE_SCALE) {
				// optimization : once a seaside miniTile verifies this condition,
				// we can throw it away as it will not generate min altitudes anymore
				utils::fast_erase(activeSeaSideList, i--);
				if (activeSeaSideList.empty()) {
					return;
				}
			} else {
				for (auto delta : { TilePosition(d.x, d.y), TilePosition(-d.x, d.y), TilePosition(d.x, -d.y), TilePosition(-d.x, -d.y),
									TilePosition(d.y, d.x), TilePosition(-d.y, d.x), TilePosition(d.y, -d.x), TilePosition(-d.y, -d.x) })
				{
					const TilePosition w = current.origin + delta;
					if (IsValid(w)) {
						CTile& tile = GetTile(w);
						if (tile.IsAltitudeMissing()) {
							tile.SetAltitude(maxAltitude = current.lastAltitudeGenerated = altitude, current.origin);
						}
					}
				}
			}
		}
	}
}

// Helper class for void Map::ComputeAreas()
// Maintains some information about an area being computed
// A TempAreaInfo is not Valid() in two cases:
//   - a default-constructed TempAreaInfo instance is never Valid (used as a dummy value to simplify the algorithm).
//   - any other instance becomes invalid when absorbed (see Merge)
class CTempAreaInfo
{
public:
						CTempAreaInfo() : m_valid(false), m_id(0), m_top(0, 0)
							, m_highestAltitude(0), m_size(0), m_corridor(false) { assert(!Valid());}
						CTempAreaInfo(CArea::Id id, CTile* pTile, TilePosition pos, bool isCorridor)
							: m_valid(true), m_id(id), m_top(pos), m_highestAltitude(pTile->GetAltitude()), m_size(0), m_corridor(isCorridor)
														{ Add(pTile); assert(Valid()); }

	bool				Valid() const					{ return m_valid; }
	CArea::Id			Id() const						{ assert(Valid()); return m_id; }
	TilePosition		Top() const						{ assert(Valid()); return m_top; }
	int					Size() const					{ assert(Valid()); return m_size; }
	CArea::Altitude		HighestAltitude() const			{ assert(Valid()); return m_highestAltitude; }
	bool				IsCorridor() const				{ return m_corridor; }

	void				Add(CTile* pTile)				{ assert(Valid()); ++m_size; pTile->SetAreaId(m_id); }

	// Left to caller : m.SetAreaId(this->Id()) for each Tile m in Absorbed
	void				Merge(CTempAreaInfo & Absorbed)	{
															assert(Valid() && Absorbed.Valid());
															assert(m_size >= Absorbed.m_size);
															m_size += Absorbed.m_size;
															Absorbed.m_valid = false;
														}

	CTempAreaInfo &		operator=(const CTempAreaInfo&) = delete;

private:
	bool					m_valid;
	const CArea::Id			m_id;
	const TilePosition		m_top;
	const CArea::Altitude	m_highestAltitude;
	int						m_size;
	bool					m_corridor;
};

// Assigns Tile::m_areaId for each tile having AreaIdMissing()
// Areas are computed using Tile::Altitude() information only.
// The tiles are considered successively in descending order of their Altitude().
// Each of them either:
//   - involves the creation of a new area.
//   - is added to some existing neighbouring area.
//   - makes two neighbouring areas merge together.
void CGridAnalyzer::ComputeAreas()
{
	std::vector<std::pair<TilePosition, CTile*>> tilesByDescendingAltitude = SortTiles();
	std::vector<CTempAreaInfo> tempAreaList = ComputeTempAreas(tilesByDescendingAltitude);
	ComputeCorridors(tempAreaList);
	CreateAreas(tempAreaList);
	grid->SetAreaIdInSectors(*this);
}

std::vector<std::pair<TilePosition, CTile*>> CGridAnalyzer::SortTiles()
{
	std::vector<std::pair<TilePosition, CTile*>> tilesByDescendingAltitude;
	for (int y = 0; y < config.tileSize.y; ++y) {
		for (int x = 0; x < config.tileSize.x; ++x) {
			const TilePosition w(x, y);
			CTile& tile = GetTile(w);
			if (tile.IsAreaIdMissing()) {
				tilesByDescendingAltitude.emplace_back(w, &tile);
			}
		}
	}

	int2 center = config.tileSize / 2;
	std::sort(tilesByDescendingAltitude.begin(), tilesByDescendingAltitude.end(),
	[center](const std::pair<TilePosition, CTile*>& a, const std::pair<TilePosition, CTile*>& b) {
		if (a.second->GetAltitude() == b.second->GetAltitude()) {
			// manhaten distance comparison
			return std::abs(a.first.x - center.x) + std::abs(a.first.y - center.y) > std::abs(b.first.x - center.x) + std::abs(b.first.y - center.y);
		}
		return a.second->GetAltitude() > b.second->GetAltitude();
	});

	return tilesByDescendingAltitude;
}

std::pair<CArea::Id, CArea::Id> CGridAnalyzer::FindNeighboringAreas(TilePosition p) const
{
	std::pair<CArea::Id, CArea::Id> result(0, 0);

	for (TilePosition delta : {TilePosition(0, -1), TilePosition(-1, 0), TilePosition(+1, 0), TilePosition(0, +1)}) {
		if (!IsValid(p + delta))
			continue;
		CArea::Id areaId = GetTile(p + delta).GetAreaId();
		if (areaId <= 0)
			continue;
		if (!result.first) result.first = areaId;
		else if (result.first != areaId)
			if (!result.second || (areaId < result.second))
				result.second = areaId;
	}

	return result;
}

CArea::Id CGridAnalyzer::ChooseNeighboringArea(CArea::Id a, CArea::Id b)
{
	if (a > b) std::swap(a, b);
	return (areaPairCounter[std::make_pair(a, b)]++ % 2 == 0) ? a : b;
}

std::vector<CTempAreaInfo> CGridAnalyzer::ComputeTempAreas(const std::vector<std::pair<TilePosition, CTile*>>& tilesByDescendingAltitude)
{
	std::vector<CTempAreaInfo> tempAreaList(1);  // TempAreaList[0] left unused, as AreaIds are > 0
	for (const auto& tCurrent : tilesByDescendingAltitude) {
		const TilePosition pos = tCurrent.first;
		CTile* cur = tCurrent.second;

		std::pair<CArea::Id, CArea::Id> neighboringAreas = FindNeighboringAreas(pos);
		if (!neighboringAreas.first) {  // no neighboring area : creates of a new area

			tempAreaList.emplace_back((CArea::Id)tempAreaList.size(), cur, pos, false);

		} else if (!neighboringAreas.second) {  // one neighboring area : adds cur to the existing area

			tempAreaList[neighboringAreas.first].Add(cur);

		} else {  // two neighboring areas : adds cur to one of them  &  possible merging

			CArea::Id smaller = neighboringAreas.first;
			CArea::Id bigger = neighboringAreas.second;
			if (tempAreaList[smaller].Size() > tempAreaList[bigger].Size()) std::swap(smaller, bigger);

			// Condition for the neighboring areas to merge:
			if ((tempAreaList[smaller].Size() < config.areasMergeSize)
				|| (tempAreaList[smaller].HighestAltitude() < config.areasMergeAltitude)
				|| (cur->GetAltitude() / (float)tempAreaList[bigger].HighestAltitude() >= 0.90f)
				|| (cur->GetAltitude() / (float)tempAreaList[smaller].HighestAltitude() >= 0.90f)
//				|| any_of(StartingLocations().begin(), StartingLocations().end(), [&pos](const TilePosition & startingLoc)
//					{ return dist(TilePosition(pos), startingLoc + TilePosition(2, 1)) <= 3;})
				|| false)
			{
				// adds cur to the absorbing area:
				tempAreaList[bigger].Add(cur);

				// merges the two neighboring areas:
				ReplaceAreaIds(tempAreaList[smaller].Top(), bigger);
				tempAreaList[bigger].Merge(tempAreaList[smaller]);

			} else {  // no merge : cur starts or continues the frontier between the two neighboring areas

				// adds cur to the chosen Area:
				tempAreaList[ChooseNeighboringArea(smaller, bigger)].Add(cur);
				rawFrontier.emplace_back(SFrontier{neighboringAreas.first, neighboringAreas.second, pos});
			}
		}
	}

	// Remove from the frontier obsolete positions
	utils::really_remove_if(rawFrontier, [](const SFrontier& f)
		{ return f.areaId1 == f.areaId2; });

	return tempAreaList;
}

void CGridAnalyzer::ComputeCorridors(std::vector<CTempAreaInfo>& tempAreas)
{
/*
	for (CTempAreaInfo& tmpArea: tempAreas) {
		tmpArea.Top() => adjacent area?
	}
	// vein test
	bool isVein = (cur->GetAltitude() <= ALTITUDE_SCALE * 20);
	if (isVein) {
		for (TilePosition delta : {TilePosition(0, -1), TilePosition(-1, 0), TilePosition(+1, 0), TilePosition(0, +1)}) {
			TilePosition neigh = pos + delta;
			if (!IsValid(neigh)) {
				continue;
			}
			CTile& tNeigh = GetTile(neigh);
			if (tNeigh.IsSea()) {
				continue;
			}
			if ((cur->GetAltitude() < tNeigh.GetAltitude()) && (tNeigh.GetAltitude() - cur->GetAltitude() > ALTITUDE_SCALE / 4)) {
				isVein = false;
				break;
			}
		}
	}
	if (isVein && !tempAreaList[neighboringAreas.first].IsCorridor()) {
		std::array<TilePosition, 8> deltas = {
			TilePosition(-2, -2), TilePosition( 0, -3), TilePosition(+2, -2),
			TilePosition(-3,  0),                       TilePosition(+3,  0),
			TilePosition(-2, +2), TilePosition( 0, +3), TilePosition(+2, +2)
		};
		CArea::Altitude maxAlt = -1;
		for (int i = 0; i < 8; ++i) {
			TilePosition test = pos + deltas[i];
			if (!IsValid(test)) {
				continue;
			}
			CTile& tTest = GetTile(test);
			if (tTest.IsSea() || tTest.IsAreaIdMissing()) {
				continue;
			}
			if (maxAlt < tTest.GetAltitude()) {
				maxAlt = tTest.GetAltitude();
			}
		}
		if (std::fabs(cur->GetAltitude() - maxAlt) < ALTITUDE_SCALE / 2) {  // isCorridor
			CArea::Id areaId = tempAreaList.size();
			tempAreaList.emplace_back(areaId, cur, pos, true);
			rawFrontier.emplace_back(SFrontier{neighboringAreas.first, areaId, pos});

std::string cmd("ai_mrk_add:");
cmd += utils::int_to_string(pos.x * 16 + 8) + " " + utils::int_to_string(pos.y * 16 + 8) + " 16 0.2 0.2 0.9 9";
gDebugLua->CallRules(cmd.c_str(), cmd.size());
cmd = std::string("ai_mrk_add:") + utils::int_to_string(cur->GetSource().x * 16 + 8) + " " + utils::int_to_string(cur->GetSource().y * 16 + 8) + " 16 0.9 0.9 0.2 9";
gDebugLua->CallRules(cmd.c_str(), cmd.size());
			continue;
		}
	}
*/
}

void CGridAnalyzer::ReplaceAreaIds(TilePosition p, CArea::Id newAreaId)
{
	CTile& tOrigin = GetTile(p);
	CArea::Id oldAreaId = tOrigin.GetAreaId();
	tOrigin.ReplaceAreaId(newAreaId);

	std::vector<TilePosition> toSearch{p};
	while (!toSearch.empty()) {
		TilePosition current = toSearch.back();

		toSearch.pop_back();
		for (TilePosition delta : {TilePosition(0, -1), TilePosition(-1, 0), TilePosition(+1, 0), TilePosition(0, +1)}) {
			TilePosition next = current + delta;
			if (IsValid(next)) {
				CTile& tNext = GetTile(next);
				if (tNext.GetAreaId() == oldAreaId) {
					toSearch.push_back(next);
					tNext.ReplaceAreaId(newAreaId);
				}
			}
		}
	}

	// also replaces references of oldAreaId by newAreaId in m_RawFrontier:
	if (newAreaId > 0) {
		for (SFrontier& f : rawFrontier) {
			if (f.areaId1 == oldAreaId) f.areaId1 = newAreaId;
			if (f.areaId2 == oldAreaId) f.areaId2 = newAreaId;
		}
	} else {
		std::remove_if(rawFrontier.begin(), rawFrontier.end(), [oldAreaId](const SFrontier& f) {
			return (f.areaId1 == oldAreaId) || (f.areaId2 == oldAreaId);
		});
	}
}

// Initializes m_Grid with the valid and big enough areas in TempAreaList.
void CGridAnalyzer::CreateAreas(const std::vector<CTempAreaInfo>& tempAreaList)
{
	std::vector<std::pair<TilePosition, int>> areasList;

	CArea::Id newAreaId = 1;
	CArea::Id newTinyAreaId = -2;

	for (const CTempAreaInfo& tempArea : tempAreaList) {
		if (tempArea.Valid()) {
			if (tempArea.Size() >= config.areaMinTiles) {
				assert(newAreaId <= tempArea.Id());
				if (newAreaId != tempArea.Id()) {
					ReplaceAreaIds(tempArea.Top(), newAreaId);
				}

				areasList.emplace_back(tempArea.Top(), tempArea.Size());
				newAreaId++;
			} else {
				ReplaceAreaIds(tempArea.Top(), newTinyAreaId);
				newTinyAreaId--;
			}
		}
	}

	grid->CreateAreas(*this, areasList);
}

} // namespace circuit
