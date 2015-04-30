/*
 * EnergyLink.h
 *
 *  Created on: Apr 29, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_
#define SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_

#include "unit/CircuitUnit.h"

#include <boost/graph/adjacency_list.hpp>
#include <map>
#include <set>

namespace circuit {

class CEnergyLink {
public:
	CEnergyLink(const springai::AIFloat3& startPos, const springai::AIFloat3& endPos);
	virtual ~CEnergyLink();

	void AddPylon(CCircuitUnit::Id unitId, const springai::AIFloat3& pos, float range);
	int RemovePylon(CCircuitUnit::Id unitId);
	void CheckConnection();

	inline bool IsBeingBuilt() { return isBeingBuilt; };
	inline bool IsFinished() { return isFinished; };
	inline bool IsValid() { return isValid; };

public:
	struct Pylon {  // Vertex
		Pylon() : pos(-RgtVector), range(.0f) {}
		Pylon(const springai::AIFloat3& p, float r) : pos(p), range(r) {}
		springai::AIFloat3 pos;
		float range;
	};
	using Graph = boost::adjacency_list<boost::listS, boost::listS, boost::undirectedS, Pylon, boost::property<boost::edge_color_t, boost::default_color_type>>;
	using VertexDesc = boost::graph_traits<Graph>::vertex_descriptor;
	using EdgeDesc = boost::graph_traits<Graph>::edge_descriptor;

//private:
	Graph graph;
	std::map<CCircuitUnit::Id, VertexDesc> pylons;
	bool isBeingBuilt;
	bool isFinished;
	bool isValid;

	springai::AIFloat3 startPos, endPos;
	std::set<VertexDesc> startPylons;
};

} // namespace circuit

#endif // SRC_CIRCUIT_RESOURCE_ENERGYLINK_H_
