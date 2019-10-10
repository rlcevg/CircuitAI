/*
 * EnergyGrid.h
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_

#include "resource/EnergyLink.h"
#include "resource/EnergyNode.h"
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
	IGridLink* GetLinkToBuild(CCircuitDef*& outDef, springai::AIFloat3& outPos);

	float GetPylonRange(CCircuitDef::Id defId);

	void SetAuthority(CCircuitAI* authority) { circuit = authority; }
private:
	CEnergyNode* FindNodeDef(CCircuitDef*& outDef, springai::AIFloat3& outPos, CEnergyNode* node);
	CEnergyLink* FindLinkDef(CCircuitDef*& outDef, springai::AIFloat3& outPos, CEnergyLink* link);

	void ReadConfig();
	void Init();
	CCircuitAI* circuit;

	int markFrame;
	std::deque<std::pair<ICoreUnit::Id, CEnergyNode*>> markedPylons;  // sorted by insertion
	std::unordered_map<CCircuitDef::Id, float> pylonRanges;
	std::map<float, CCircuitDef::Id> rangePylons;

	std::vector<bool> linkedClusters;
	std::set<int> linkPylons, unlinkPylons;
	std::set<int> linkNodes, unlinkNodes;
	std::vector<CEnergyLink> links;  // Graph's exterior property
	std::vector<CEnergyNode*> nodes;  // Graph's exterior property

	void MarkAllyPylons(const std::vector<CAllyUnit*>& pylons);
	CEnergyNode* AddPylon(ICoreUnit::Id unitId, CCircuitDef::Id defId, const springai::AIFloat3& pos);
	void RemovePylon(const std::pair<ICoreUnit::Id, CEnergyNode*>& pylonId);
	void CheckGrid();

	std::vector<int> linkClusters;
	std::vector<int> unlinkClusters;
	bool isForceRebuild;

	class SpanningLink;
	class DetectLink;
	class DetectNode;
	using OwnedFilter = CMetalData::ClusterGraph::NodeMap<bool>;
	using OwnedGraph = lemon::FilterNodes<const CMetalData::ClusterGraph, OwnedFilter>;
	using SpanningTree = std::set<CMetalData::ClusterGraph::Edge>;
	using SpanningGraph = lemon::FilterEdges<const CMetalData::ClusterGraph, SpanningLink>;
	using SpanningBFS = lemon::Bfs<SpanningGraph>;

	SpanningTree spanningTree;
	OwnedFilter* ownedFilter;
	OwnedGraph* ownedClusters;
	CMetalData::ClusterCostMap* edgeCosts;

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
