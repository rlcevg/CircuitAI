/*
 * EnergyLink.cpp
 *
 *  Created on: Apr 19, 2015
 *      Author: rlcevg
 */

#include "resource/EnergyLink.h"
#include "resource/MetalManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include <algorithm>

namespace circuit {

using namespace springai;

CEnergyLink::CEnergyLink(CCircuitAI* circuit) :
		circuit(circuit),
		markFrame(-1)
{
	for (auto& kv : circuit->GetCircuitDefs()) {
		const std::map<std::string, std::string>& customParams = kv.second->GetUnitDef()->GetCustomParams();
		auto it = customParams.find("pylonrange");
		if (it != customParams.end()) {
			pylonDefIds[kv.first] = utils::string_to_float(it->second);
		}
	}
}

CEnergyLink::~CEnergyLink()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CEnergyLink::Update()
{
	circuit->UpdateFriendlyUnits();

	MarkAllyPylons();
}

void CEnergyLink::MarkAllyPylons()
{
	int lastFrame = circuit->GetLastFrame();
	if (markFrame + FRAMES_PER_SEC >= lastFrame) {
		return;
	}
	markFrame = lastFrame;

	circuit->UpdateFriendlyUnits();
	const CAllyTeam::Units& friendlies = circuit->GetFriendlyUnits();

	std::set<Structure, cmp> newUnits, oldUnits;
	for (auto& kv : friendlies) {
		CCircuitUnit* unit = kv.second;
		Unit* u = unit->GetUnit();
		if (u->GetMaxSpeed() <= 0) {
			Structure building;
			building.unitId = kv.first;
			decltype(markedAllies)::iterator it = markedAllies.find(building);
			if (it == markedAllies.end()) {
				building.cdefId = unit->GetCircuitDef()->GetId();
				building.pos = u->GetPos();

				decltype(newUnits)::iterator newIt;
				bool ok;
				std::tie(newIt, ok) = newUnits.insert(building);
				if (ok) {
					MarkPylon(*newIt, true);
				}
			} else {
				oldUnits.insert(*it);
			}
		}
	}
	std::set<Structure, cmp> deadUnits;
	std::set_difference(markedAllies.begin(), markedAllies.end(),
						oldUnits.begin(), oldUnits.end(),
						std::inserter(deadUnits, deadUnits.begin()), cmp());
	for (auto& building : deadUnits) {
		MarkPylon(building, false);
	}
	markedAllies.clear();
	std::set_union(oldUnits.begin(), oldUnits.end(),
				   newUnits.begin(), newUnits.end(),
				   std::inserter(markedAllies, markedAllies.begin()), cmp());
}

void CEnergyLink::MarkPylon(const Structure& building, bool alive)
{
	CMetalManager* metalManager = circuit->GetMetalManager();
	int index = metalManager->FindNearestCluster(building.pos);
	if (index < 0) {
		return;
	}
	const CMetalData::Clusters& clusters = metalManager->GetClusters();
	const CMetalData::Graph& clusterGraph = metalManager->GetGraph();

	if (alive) {
		const AIFloat3& P = building.pos;
		const AIFloat3& P0 = clusters[index].geoCentr;
		auto sqDistPointToLine = [&P0, &P](const AIFloat3& P1) {
			float A = P0.z - P1.z;
			float B = P1.x - P0.x;
			float C = P0.x * P1.z - P1.x * P0.z;
			float denominator = A * P.x + B * P.z + C;
			float numerator = A * A + B * B;
			return denominator * denominator / numerator;
		};

		// Find edge to which building belongs to: pylonEdge
		CMetalData::Graph::out_edge_iterator edgeIt, edgeEnd, pylonEdge;
		std::tie(edgeIt, edgeEnd) = boost::out_edges(index, clusterGraph);  // or boost::tie
		float sqMinDist = std::numeric_limits<float>::max();
		for (; edgeIt != edgeEnd; ++edgeIt) {
			int idxTarget = boost::target(*edgeIt, clusterGraph);
			float sqDist = sqDistPointToLine(clusters[idxTarget].geoCentr);
			if (sqDist < sqMinDist) {
				pylonEdge = edgeIt;
				sqMinDist = sqDist;
			}
		}

	} else {


	}

//	CMetalData::Edge& edge = clusterGraph[*pylonEdge];
//	// On insert
//	edge.insert();
//	if (edge.beingBuilt) {
//		// check if its finished!
//	}
//	// On erase
//	edge.erase();
//	edge.beingBuilt = numPylons > 0;
//	if (edge.beingBuilt) {
//		// check if its finished!
//	}
}

} // namespace circuit
