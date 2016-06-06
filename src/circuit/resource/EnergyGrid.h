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

#include <set>
#include <map>
#include <unordered_map>

namespace circuit {

class CCircuitAI;

class CEnergyGrid {
public:
	typedef boost::property_map<CMetalData::Graph, int CMetalData::SEdge::*>::const_type EdgeIndexMap;
	using link_iterator_t = boost::iterator_property_map<CEnergyLink*, EdgeIndexMap, CEnergyLink, CEnergyLink&>;

	CEnergyGrid(CCircuitAI* circuit);
	virtual ~CEnergyGrid();

	void Update();
	void SetForceRebuild(bool value) { isForceRebuild = value; }
	CEnergyLink* GetLinkToBuild(CCircuitDef*& outDef, springai::AIFloat3& outPos);

	float GetPylonRange(CCircuitDef::Id defId);

private:
	void Init();
	CCircuitAI* circuit;

	int markFrame;
	std::deque<CCircuitUnit::Id> markedPylons;  // sorted by insertion
	std::unordered_map<CCircuitDef::Id, float> pylonRanges;
	std::map<float, CCircuitDef::Id> rangePylons;

	std::vector<bool> linkedClusters;
	std::set<CMetalData::EdgeDesc> linkPylons, unlinkPylons;
	std::vector<CEnergyLink> links;  // Graph's exterior property
	link_iterator_t linkIt;  // Alternative: links[clusterGraph[*linkEdgeIt].index]

	void MarkAllyPylons(const std::vector<CCircuitUnit*>& pylons);
	void AddPylon(CCircuitUnit::Id unitId, CCircuitDef::Id defId, const springai::AIFloat3& pos);
	void RemovePylon(CCircuitUnit::Id unitId);
	void CheckGrid();

	std::vector<int> linkClusters;
	std::vector<int> unlinkClusters;
	std::set<CMetalData::EdgeDesc> spanningTree;
	CMetalData::Graph ownedClusters;
	bool isForceRebuild;

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
