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

CEnergyLink::CEnergyLink(int idx0, const AIFloat3& P0, int idx1, const AIFloat3& P1) :
		isBeingBuilt(false),
		isFinished(false),
		isValid(true),
		v0(new SVertex(idx0, P0)),
		v1(new SVertex(idx1, P1))
{
}

CEnergyLink::~CEnergyLink()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	for (auto& kv : pylons) {
		delete kv.second;
	}
	delete v0, v1;
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

	float sqRange = range * range;
	if (v0->pos.SqDistance2D(pos) < sqRange) {
		v0->pylons.insert(pylon0);
	}
	if (v1->pos.SqDistance2D(pos) < sqRange) {
		v1->pylons.insert(pylon0);
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
	v0->pylons.erase(pylon0);
	v1->pylons.erase(pylon0);
	delete pylon0;

	return pylons.erase(unitId);
}

void CEnergyLink::CheckConnection()
{
//	int i = 0;
	std::set<SPylon*> visited;
	std::queue<SPylon*> queue;

	for (SPylon* p : v0->pylons) {
		queue.push(p);
	}

	while (!queue.empty()) {
		SPylon* q = queue.front();
		queue.pop();
		float dist = q->range;
		if (v1->pos.SqDistance2D(q->pos) < dist * dist) {
			isFinished = true;
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

void CEnergyLink::CalcHeadInfos(SBuildInfo& outInfo0, SBuildInfo& outInfo1)
{
	SPylon* pylon0 = GetConnectionHead(v0, v1);
	SPylon* pylon1 = GetConnectionHead(v1, v0);
	outInfo0.pos   = (pylon0 != nullptr) ? pylon0->pos   : v0->pos;
	outInfo0.range = (pylon0 != nullptr) ? pylon0->range : .0f;
	outInfo1.pos   = (pylon1 != nullptr) ? pylon1->pos   : v1->pos;
	outInfo1.range = (pylon1 != nullptr) ? pylon1->range : .0f;
}

void CEnergyLink::SetStartVertex(int index)
{
	if (index != v0->index) {
		std::swap(v0, v1);
	}
}

CEnergyLink::SPylon* CEnergyLink::GetConnectionHead(SVertex* v0, SVertex* v1)
{
	SPylon* winner = nullptr;
	float sqMinDist = std::numeric_limits<float>::max();

//	int i = 0;
	std::set<SPylon*> visited;
	std::queue<SPylon*> queue;

	for (SPylon* p : v0->pylons) {
		queue.push(p);
	}

	while (!queue.empty()) {
		SPylon* q = queue.front();
		queue.pop();
		float sqDist = v1->pos.SqDistance2D(q->pos);
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
