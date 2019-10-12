/*
 * EnergyNode.cpp
 *
 *  Created on: Oct 5, 2019
 *      Author: rlcevg
 */

#include "resource/EnergyNode.h"
#include "util/utils.h"
#include "lemon/kruskal.h"
#include "lemon/adaptors.h"
#include "lemon/bfs.h"

namespace circuit {

using namespace springai;

class ExamineNode : public lemon::MapBase<CEnergyNode::SpotGraph::Node, bool> {
public:
	ExamineNode(CEnergyNode::SpotGraph& graph, CEnergyNode::SpotNodeMap& spotNodes,
		const CEnergyNode::SpotNodeMap& nodes)
		: graph(graph), spotNodes(spotNodes), nodes(nodes)
	{}
	bool operator[](Key k) const {
		spotNodes[graph.addNode()] = nodes[k];
		return false;
	}
private:
	CEnergyNode::SpotGraph& graph;
	CEnergyNode::SpotNodeMap& spotNodes;
	const CEnergyNode::SpotNodeMap& nodes;
};

CEnergyNode::CEnergyNode(int index, const CMetalData::SCluster& cluster, const CMetalData::Metals& spots)
		: IGridLink()
		, spotNodes(spotGraph)
		, spotEdgeCosts(spotGraph)
		, info(index, cluster.idxSpots.size(), cluster.position)
{
	SpotGraph initGraph;
	SpotNodeMap initNodeMap(initGraph);
	for (const int i : cluster.idxSpots) {
		initNodeMap[initGraph.addNode()].pos = spots[i].position;
	}

	SpotCostMap tmpEdgeCosts(initGraph);
	BuildMexGraph(initGraph, tmpEdgeCosts, cluster, spots);

	SpotGraph::EdgeMap<bool> spanningTree(initGraph);
	lemon::kruskal(initGraph, tmpEdgeCosts, spanningTree);

	// Sort initial mexes in the order of bfs traversing minimal spanning tree
	ExamineNode vis(spotGraph, spotNodes, initNodeMap);
	SpotGraph::NodeIt node(initGraph);
	vis[node];
	auto spanningGraph = lemon::filterEdges(initGraph, spanningTree);
	lemon::Bfs<typeof(spanningGraph)> bfs(spanningGraph);
	bfs.init();
	bfs.addSource(node);
	bfs.start(vis);
}

CEnergyNode::~CEnergyNode()
{
}

bool CEnergyNode::AddPylon(ICoreUnit::Id unitId, const springai::AIFloat3& pos, float range)
{
	if (pylons.find(unitId) != pylons.end()) {
		return false;
	}

	SpotGraph::Node node0 = spotGraph.addNode();
	spotNodes[node0] = {pos, range};

	SpotGraph::NodeIt nodeIt(spotGraph);
	for (; nodeIt != lemon::INVALID; ++nodeIt) {
		const SPylon& pylon1 = spotNodes[nodeIt];
		const float dist = range + pylon1.range;
		if (pos.SqDistance2D(pylon1.pos) < SQUARE(dist)) {
			spotGraph.addEdge(node0, nodeIt);
		}
	}

	if (info.pos.SqDistance2D(pos) < SQUARE(range)) {
		info.neighbors.insert(unitId);
	}

	pylons[unitId] = node0;
	return true;
}

bool CEnergyNode::RemovePylon(ICoreUnit::Id unitId)
{
	auto it = pylons.find(unitId);
	if (it == pylons.end()) {
		return false;
	}
	spotGraph.erase(it->second);

	info.neighbors.erase(unitId);

	return it != pylons.erase(it);
}

void CEnergyNode::CheckConnection()
{
	SpotGraph::NodeIt t(spotGraph);
	lemon::Bfs<SpotGraph> bfs(spotGraph);
	bfs.init();
	bfs.addSource(t);
	bfs.start();
	for (++t; t != lemon::INVALID; ++t) {
		if (!bfs.reached(t)) {
			break;
		}
	}
	if (t == lemon::INVALID) {
		isFinished = true;
		return;
	}

	target = t;
	source = lemon::INVALID;

	isFinished = false;
}

const CEnergyNode::SPylon& CEnergyNode::GetSourceHead()
{
	if (source != lemon::INVALID) {
		return spotNodes[source];
	}
	SpotGraph::Node s = SpotGraph::NodeIt(spotGraph);
	const AIFloat3& P1 = spotNodes[target].pos;
	float minDist = std::numeric_limits<float>::max();
	lemon::Bfs<SpotGraph> bfs(spotGraph);
	bfs.init();
	bfs.addSource(s);
	while (!bfs.emptyQueue()) {
		SpotGraph::Node node = bfs.nextNode();
		const SPylon& q = spotNodes[node];
		float dist = P1.distance2D(q.pos) - q.range;
		if (dist < minDist) {
			minDist = dist;
			s = node;
		}
		bfs.processNextNode();
	}
	source = s;
	return spotNodes[source];
}

bool CEnergyNode::IsPylonable() const
{
	return info.neighbors.empty() && info.size > 2;
}

void CEnergyNode::BuildMexGraph(SpotGraph& graph, SpotCostMap& edgeCosts,
		const CMetalData::SCluster& cluster, const CMetalData::Metals& spots)
{
	const CMetalData::MetalIndices& indices = cluster.idxSpots;
	auto addEdge = [&graph, &edgeCosts, &indices, &spots](std::size_t A, std::size_t B) -> void {
		SpotGraph::Edge edge = graph.addEdge(graph.nodeFromId(A), graph.nodeFromId(B));
		edgeCosts[edge] = spots[indices[A]].position.distance2D(spots[indices[B]].position);
	};
	if (indices.size() < 2) {
		// do nothing
	} else if (indices.size() < 3) {
		addEdge(0, 1);
	} else {
		// Mex triangulation
		std::vector<double> coords;
		coords.reserve(indices.size() * 2);  // 2D
		for (const int i : indices) {
			coords.push_back(spots[i].position.x);
			coords.push_back(spots[i].position.z);
		}
		CMetalData::TriangulateGraph(coords, [&indices, &spots](std::size_t A, std::size_t B) -> float {
			return spots[indices[A]].position.distance2D(spots[indices[B]].position);
		}, addEdge);
	}
}

} // namespace circuit
