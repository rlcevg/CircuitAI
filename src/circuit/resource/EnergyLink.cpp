/*
 * EnergyLink.cpp
 *
 *  Created on: Apr 29, 2015
 *      Author: rlcevg
 *        Note: Alternative easier/faster implementation - https://github.com/Anarchid/zkgbai/blob/master/src/zkgbai/graph/Link.java
 */

#include "resource/EnergyLink.h"
#include "util/utils.h"

#include <boost/graph/depth_first_search.hpp>
#include <exception>

#define PYLON_RANGE 500.0f

namespace circuit {

using namespace springai;

class CExitDFS: public std::exception {
	virtual const char* what() const throw() {
		return "DFS goal has been reached";
	}
} exitDFS;

class detect_link : public boost::dfs_visitor<> {
public:
	detect_link(const AIFloat3& pos, std::set<CEnergyLink::VertexDesc>& pylons) : clPos(pos), startPylons(&pylons) {}
	template <class Vertex, class Graph>
	void discover_vertex(Vertex u, const Graph& g) {
		const CEnergyLink::Pylon& pylon = g[u];
		float range = pylon.range + PYLON_RANGE;  // FIXME: Remove const
		if (clPos.SqDistance2D(pylon.pos) < range * range) {
			throw exitDFS;
		}
		auto it = startPylons->find(u);
		if (it != startPylons->end()) {
			startPylons->erase(it);
		}
	}
	AIFloat3 clPos;
	std::set<CEnergyLink::VertexDesc>* startPylons;
};

CEnergyLink::CEnergyLink(const AIFloat3& startPos, const AIFloat3& endPos) :
		isBeingBuilt(false),
		isFinished(false),
		isValid(true),
		startPos(startPos),
		endPos(endPos)
{
}

CEnergyLink::~CEnergyLink()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CEnergyLink::AddPylon(CCircuitUnit::Id unitId, const AIFloat3& pos, float range)
{
	VertexDesc pylon0 = boost::add_vertex(Pylon(pos, range), graph);

	Graph::vertex_iterator vertIt, vertEnd;
	std::tie(vertIt, vertEnd) = boost::vertices(graph);
	for (; vertIt != vertEnd; ++vertIt) {
		VertexDesc pylon1 = *vertIt;
		if (pylon0 == pylon1) {
			continue;
		}
		Pylon& data1 = graph[pylon1];
		float totalRange = range + data1.range;
		if (pos.SqDistance2D(data1.pos) < totalRange * totalRange) {
			boost::add_edge(pylon0, pylon1, graph);
		}
	}

	pylons[unitId] = pylon0;
	float exRange = range + PYLON_RANGE;
	if (startPos.SqDistance2D(pos) < exRange * exRange) {
		startPylons.insert(pylon0);
	}
}

int CEnergyLink::RemovePylon(CCircuitUnit::Id unitId)
{
	auto it = pylons.find(unitId);
	if (it == pylons.end()) {
		return 0;
	}
	VertexDesc pylon = it->second;

	boost::clear_vertex(pylon, graph);
	boost::remove_vertex(pylon, graph);

	startPylons.erase(pylon);
	return pylons.erase(unitId);
}

void CEnergyLink::CheckConnection()
{
	bool fin = false;
	detect_link vis(endPos, startPylons);
	try {  // FIXME: Replace exception hax with own depth_first_search implementation
		auto it = startPylons.begin();
		std::set<VertexDesc> pyls = startPylons;
		while (!pyls.empty()) {
			VertexDesc vertId = *pyls.begin();
			boost::depth_first_search(graph, boost::root_vertex(vertId).visitor(vis));
		}
	} catch (const CExitDFS& e) {
		fin = true;
	}
	// TODO: Check if there are end-pylons that contains all cluster spots
	isFinished = fin;

	printf("%i\n", isFinished);
}

} // namespace circuit
