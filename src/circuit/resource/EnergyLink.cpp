/*
 * EnergyLink.cpp
 *
 *  Created on: Apr 29, 2015
 *      Author: rlcevg
 *      Original implementation by Anarchid: https://github.com/Anarchid/zkgbai/blob/master/src/zkgbai/graph/Link.java
 */

#include "resource/EnergyLink.h"
#include "util/utils.h"

#include <queue>

namespace circuit {

using namespace springai;

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
	for (auto& kv : pylons) {
		delete kv.second;
	}
}

void CEnergyLink::AddPylon(CCircuitUnit::Id unitId, const AIFloat3& pos, float range)
{
	SPylon* pylon0 = new SPylon(pos, range);

	for (auto& kv : pylons) {
		SPylon* pylon1 = kv.second;
		float dist = range + pylon1->range;
		if (pos.SqDistance2D(pylon1->pos) < dist * dist) {
			pylon0->neighbors.insert(pylon1);
			pylon1->neighbors.insert(pylon0);
		}
	}

	pylons[unitId] = pylon0;

	float exRange = range;/* + PYLON_RANGE;*/
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
	SPylon* pylon0 = it->second;

	for (SPylon* pylon1 : pylon0->neighbors) {
		pylon1->neighbors.erase(pylon0);
	}
	startPylons.erase(pylon0);
	delete pylon0;

	return pylons.erase(unitId);
}

void CEnergyLink::CheckConnection()
{
//	int i = 0;
	std::set<SPylon*> visited;
	std::queue<SPylon*> queue;

	for (SPylon* p : startPylons) {
		queue.push(p);
	}

	while (!queue.empty()) {
		SPylon* q = queue.front();
		queue.pop();
		float dist = q->range;/* + PYLON_RANGE;  // FIXME: Remove const*/
		if (endPos.SqDistance2D(q->pos) < dist * dist) {
			isFinished = true;
			// TODO: Check if there are end-pylons that contains all spots in cluster
			return;
		}

//		if (i++ > 1000) break;

		visited.insert(q);
		for (SPylon* child : q->neighbors) {
			if (visited.find(child) == visited.end()) {
				queue.push(child);
			}
		}
	}

	isFinished = false;
}

CEnergyLink::SPylon* CEnergyLink::GetConnectionHead()
{
	SPylon* winner = nullptr;
	float sqMinDist = std::numeric_limits<float>::max();

//	int i = 0;
	std::set<SPylon*> visited;
	std::queue<SPylon*> queue;

	for (SPylon* p : startPylons) {
		queue.push(p);
	}

	while (!queue.empty()) {
		SPylon* q = queue.front();
		queue.pop();
		float sqDist = endPos.SqDistance2D(q->pos);
		if (sqDist < sqMinDist) {
			sqMinDist = sqDist;
			winner = q;
		}

//		if (i++ > 1000) return winner;

		visited.insert(q);
		for (SPylon* child : q->neighbors) {
			if (visited.find(child) == visited.end()) {
				queue.push(child);
			}
		}
	}

	return winner;
}

} // namespace circuit
