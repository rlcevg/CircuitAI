/*
 * EnergyGrid.h
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_

#include "resource/EnergyLink.h"
#include "resource/MetalData.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "lemon/adaptors.h"
#include "lemon/bfs.h"

#include <set>
#include <map>
#include <unordered_map>

namespace circuit {

class CCircuitAI;

class CEnergyGrid {
public:
	CEnergyGrid(CCircuitAI* circuit);
	virtual ~CEnergyGrid();

	void Update();
	void SetForceRebuild(bool value) { isForceRebuild = value; }
	CEnergyLink* GetLinkToBuild(CCircuitDef*& outDef, springai::AIFloat3& outPos);

	float GetPylonRange(CCircuitDef::Id defId);

	void SetAuthority(CCircuitAI* authority) { circuit = authority; }
private:
	void ReadConfig();
	void Init();
	CCircuitAI* circuit;

	int markFrame;
	std::deque<ICoreUnit::Id> markedPylons;  // sorted by insertion
	std::unordered_map<CCircuitDef::Id, float> pylonRanges;
	std::map<float, CCircuitDef::Id> rangePylons;

	std::vector<bool> linkedClusters;
	std::set<int> linkPylons, unlinkPylons;
	std::vector<CEnergyLink> links;  // Graph's exterior property

	void MarkAllyPylons(const std::vector<CAllyUnit*>& pylons);
	void AddPylon(ICoreUnit::Id unitId, CCircuitDef::Id defId, const springai::AIFloat3& pos);
	void RemovePylon(ICoreUnit::Id unitId);
	void CheckGrid();

	std::vector<int> linkClusters;
	std::vector<int> unlinkClusters;
	bool isForceRebuild;

	class SpanningLink;
	class DetectLink;
	using OwnedFilter = CMetalData::Graph::NodeMap<bool>;
	using OwnedGraph = lemon::FilterNodes<const CMetalData::Graph, OwnedFilter>;
	using SpanningTree = std::set<CMetalData::Graph::Edge>;
	using SpanningGraph = lemon::FilterEdges<const CMetalData::Graph, SpanningLink>;
	using SpanningBFS = lemon::Bfs<SpanningGraph>;

	SpanningTree spanningTree;
	OwnedFilter* ownedFilter;
	OwnedGraph* ownedClusters;
	CMetalData::WeightMap* edgeCosts;

	SpanningLink* spanningFilter;
	SpanningGraph* spanningGraph;
	SpanningBFS* spanningBfs;  // breadth-first search

	void MarkClusters();
	void RebuildTree();

#ifdef DEBUG_VIS
private:
	int figureGridId;
	int figureInvalidId;
	int figureFinishedId;
	int figureBuildId;
	int figureKruskalId;
	bool isVis;
	int toggleFrame;
public:
	bool IsVis() const { return isVis; }
	void UpdateVis();
	void ToggleVis();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_
