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
	typedef boost::property_map<CMetalData::Graph, int CMetalData::Edge::*>::const_type EdgeIndexMap;
	using link_iterator_t = boost::iterator_property_map<CEnergyLink*, EdgeIndexMap, CEnergyLink, CEnergyLink&>;

	CEnergyGrid(CCircuitAI* circuit);
	virtual ~CEnergyGrid();

	void Update();
	inline void SetForceRebuild(bool value) { isForceRebuild = value; }
	CEnergyLink* GetLinkToBuild();

private:
	void Init();
	CCircuitAI* circuit;

	int markFrame;
	using Structures = std::map<CCircuitUnit::Id, springai::AIFloat3>;
	Structures markedPylons;
	std::unordered_map<CCircuitDef::Id, float> pylonRanges;

	Structures markedMexes;
	struct SLinkVertex {
		int mexCount;
		bool isConnected;
	};
	std::vector<SLinkVertex> linkedClusters;
	std::set<CMetalData::EdgeDesc> linkPylons, unlinkPylons;
	std::vector<CEnergyLink> links;  // Graph's exterior property
	link_iterator_t linkIt;  // Alternative: links[clusterGraph[*linkEdgeIt].index]

	void MarkAllyPylons(const std::list<CCircuitUnit*>& pylons);
	void AddPylon(CCircuitUnit* unit, const springai::AIFloat3& pos);
	void RemovePylon(CCircuitUnit::Id unitId, const springai::AIFloat3& pos);
	void CheckGrid();

	std::list<int> linkClusters, unlinkClusters;  // - must not contain same clusters
	std::set<CMetalData::EdgeDesc> spanningTree;
	CMetalData::Graph ownedClusters;
	bool isForceRebuild;

	void MarkAllyMexes(const std::list<CCircuitUnit*>& mexes);
	void AddMex(const springai::AIFloat3& pos);
	void RemoveMex(const springai::AIFloat3& pos);
	void RebuildTree();
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYGRID_H_
