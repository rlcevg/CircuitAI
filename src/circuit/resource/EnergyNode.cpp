/*
 * EnergyNode.cpp
 *
 *  Created on: Oct 5, 2019
 *      Author: rlcevg
 */

#include "resource/EnergyNode.h"
#include "util/Utils.h"
#include "lemon/kruskal.h"
#include "lemon/adaptors.h"
#include "lemon/bfs.h"

namespace circuit {

using namespace springai;

class BuildNode : public lemon::MapBase<CEnergyNode::SpotGraph::Node, bool> {
public:
	BuildNode(CEnergyNode::SpotGraph& graph, CEnergyNode::SpotNodeMap& spotNodes,
		const CEnergyNode::SpotNodeMap& nodes)
		: graph(graph), spotNodes(spotNodes), nodes(nodes)
	{}
	Value operator[](Key k) const {
		spotNodes[graph.addNode()] = nodes[k];
		return false;
	}
private:
	CEnergyNode::SpotGraph& graph;
	CEnergyNode::SpotNodeMap& spotNodes;
	const CEnergyNode::SpotNodeMap& nodes;
};

class ExamineNode : public lemon::MapBase<CEnergyNode::SpotGraph::Node, bool> {
public:
	ExamineNode(const std::set<CEnergyNode::SpotGraph::Node>& mexes, int& connected)
		: mexes(mexes), connected(connected)
	{}
	Value operator[](Key k) const {
		if (mexes.find(k) != mexes.end()) {
			++connected;
		}
		return false;
	}
private:
	const std::set<CEnergyNode::SpotGraph::Node>& mexes;
	int& connected;
};

CEnergyNode::CEnergyNode(int index, const CMetalData::SCluster& cluster, const CMetalData::Metals& spots)
		: IGridLink()
		, spotNodes(spotGraph)
		, spotEdgeCosts(spotGraph)
		, isMexed(false)
		, info(index, cluster.position, cluster.radius)
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
	BuildNode vis(spotGraph, spotNodes, initNodeMap);
	SpotGraph::NodeIt node(initGraph);
	auto spanningGraph = lemon::filterEdges(initGraph, spanningTree);
	lemon::Bfs<__typeof__(spanningGraph)> bfs(spanningGraph);
	bfs.init();
	bfs.addSource(node);
	vis[node];
	bfs.start(vis);  // FIXME: sorting won't work as nodes are probably stored in some std::set container

	for (SpotGraph::NodeIt nodeIt(spotGraph); nodeIt != lemon::INVALID; ++nodeIt) {
		info.mexes.insert(nodeIt);
	}
	target = *info.mexes.begin();
	source = lemon::INVALID;
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
	SpotGraph::Node s = *info.mexes.begin();
	int mexConnected = 0;
	ExamineNode countMex(info.mexes, mexConnected);
	lemon::Bfs<SpotGraph> bfs(spotGraph);
	bfs.init();
	bfs.addSource(s);
	countMex[s];
	bfs.start(countMex);
	SpotGraph::NodeIt t(spotGraph);
	for (; t != lemon::INVALID; ++t) {
		if (!bfs.reached(t)) {
			break;
		}
	}
	if (t == lemon::INVALID) {
		isMexed = isFinished = true;
		target = source = s;
		return;
	}

	target = t;
	source = lemon::INVALID;

	isMexed = mexConnected >= (int)info.mexes.size();
	isFinished = false;
}

const CEnergyNode::SPylon& CEnergyNode::GetSourceHead()
{
	if (source != lemon::INVALID) {
		return spotNodes[source];
	}
	SpotGraph::Node s = *info.mexes.begin();
	const AIFloat3& P1 = spotNodes[target].pos;
	float minDist = P1.distance2D(spotNodes[s].pos) - spotNodes[s].range;
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

bool CEnergyNode::IsPylonable(float radius) const
{
	return info.neighbors.empty() && (info.mexes.size() > 2) && (info.radius > radius * 0.6f);
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
