/*
 * EnergyNode.h
 *
 *  Created on: Oct 5, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYNODE_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYNODE_H_

#include "resource/GridLink.h"
#include "resource/MetalData.h"
#include "unit/CoreUnit.h"
#include "lemon/list_graph.h"

namespace circuit {

class CEnergyNode: public IGridLink {
public:
	struct SPylon {
		SPylon() : pos(-RgtVector), range(0.f) {}
		SPylon(const springai::AIFloat3& p, float r) : pos(p), range(r) {}
		springai::AIFloat3 pos;
		float range;
	};
	using SpotGraph = lemon::ListGraph;
	using SpotNodeMap = SpotGraph::NodeMap<SPylon>;
	using SpotCostMap = SpotGraph::EdgeMap<float>;

	CEnergyNode(const CMetalData::SCluster& cluster, const CMetalData::Metals& spots);
	virtual ~CEnergyNode();

	void AddPylon(ICoreUnit::Id unitId, const springai::AIFloat3& pos, float range);
	bool RemovePylon(ICoreUnit::Id unitId);
	void CheckConnection();
	const SPylon& GetSourceHead() const { return spotNodes[source]; }
	const SPylon& GetTargetHead() const { return spotNodes[target]; }

private:
	void BuildMexGraph(SpotGraph& graph, SpotCostMap& edgeCosts,
			const CMetalData::SCluster& cluster, const CMetalData::Metals& spots);

	SpotGraph spotGraph;
	SpotNodeMap spotNodes;
	SpotCostMap spotEdgeCosts;

	std::map<ICoreUnit::Id, SpotGraph::Node> pylons;

	SpotGraph::Node source, target;
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYNODE_H_
