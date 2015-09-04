/*
 * EnergyGrid.h
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_

#include "resource/EnergyLink.h"
#include "static/MetalData.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"

#include <set>
#include <map>
#include <unordered_map>
#include <list>

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
	using Structures = std::map<CCircuitUnit::Id, springai::AIFloat3>;
	Structures markedPylons;
	std::unordered_map<CCircuitDef::Id, float> pylonRanges;
	std::map<float, CCircuitDef::Id> rangePylons;

	Structures markedMexes;
	struct SLinkVertex {
		void SetConnected(bool value) { oldConnected = isConnected; isConnected = value; }
		void RevertConnected() { isConnected = oldConnected; }
		int mexCount;
		bool isConnected, oldConnected;
	};
	std::vector<SLinkVertex> linkedClusters;
	std::set<CMetalData::EdgeDesc> linkPylons, unlinkPylons;
	std::vector<CEnergyLink> links;  // Graph's exterior property
	link_iterator_t linkIt;  // Alternative: links[clusterGraph[*linkEdgeIt].index]

	void MarkAllyPylons(const std::list<CCircuitUnit*>& pylons);
	void AddPylon(CCircuitUnit::Id unitId, CCircuitDef::Id defId, const springai::AIFloat3& pos);
	void RemovePylon(CCircuitUnit::Id unitId);
	void CheckGrid();

	std::set<int> linkClusters;
	std::list<int> unlinkClusters;
	std::set<CMetalData::EdgeDesc> spanningTree;
	CMetalData::Graph ownedClusters;
	bool isForceRebuild;

	void MarkAllyMexes(const std::list<CCircuitUnit*>& mexes);
	void AddMex(const springai::AIFloat3& pos);
	void RemoveMex(const springai::AIFloat3& pos);
	void RebuildTree();

#ifdef DEBUG_VIS
private:
	int figureGridId;
	int figureInvalidId;
	int figureFinishedId;
	int figureBuildId;
	bool drawGrid;
	int toggleFrame;
	void UpdateVis();
public:
	void ToggleVis();
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_
