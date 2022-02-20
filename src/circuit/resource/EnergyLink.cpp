/*
 * EnergyLink.cpp
 *
 *  Created on: Apr 29, 2015
 *      Author: rlcevg
 */

#include "resource/EnergyLink.h"
#include "util/Utils.h"

#include <queue>

namespace circuit {

using namespace springai;

CEnergyLink::CEnergyLink(int idx0, const AIFloat3& P0, int idx1, const AIFloat3& P1)
		: IGridLink()
		, source(new SVertex(idx0, P0))
		, target(new SVertex(idx1, P1))
		, invDistance(1.f / P0.distance2D(P1))
		, costMod(1.f)
{
}

CEnergyLink::~CEnergyLink()
{
	for (auto& kv : pylons) {
		delete kv.second;
	}
	delete source;
	delete target;
}

void CEnergyLink::AddPylon(ICoreUnit::Id unitId, const AIFloat3& pos, float range)
{
	if (pylons.find(unitId) != pylons.end()) {
		return;
	}

	SPylon* pylon0 = new SPylon(pos, range);

	for (auto& kv : pylons) {
		SPylon* pylon1 = kv.second;
		float dist = range + pylon1->range;
		if (pos.SqDistance2D(pylon1->pos) < SQUARE(dist)) {
			pylon0->neighbors.insert(pylon1);
			pylon1->neighbors.insert(pylon0);
		}
	}

	pylons[unitId] = pylon0;

	if (source->pylon.pos.SqDistance2D(pos) < SQUARE(range)) {
		source->pylon.neighbors.insert(pylon0);
		pylon0->neighbors.insert(&source->pylon);
	}
	if (target->pylon.pos.SqDistance2D(pos) < SQUARE(range)) {
		target->pylon.neighbors.insert(pylon0);
		pylon0->neighbors.insert(&target->pylon);
	}
}

bool CEnergyLink::RemovePylon(ICoreUnit::Id unitId)
{
	auto it = pylons.find(unitId);
	if (it == pylons.end()) {
		return false;
	}
	SPylon* pylon0 = it->second;

	for (SPylon* pylon1 : pylon0->neighbors) {
		pylon1->neighbors.erase(pylon0);
	}
	source->pylon.neighbors.erase(pylon0);
	target->pylon.neighbors.erase(pylon0);
	delete pylon0;

	return it != pylons.erase(it);
}

void CEnergyLink::CheckConnection()
{
	SPylon* sourceHead = nullptr;
	SPylon* targetHead = nullptr;
	float minDist = std::numeric_limits<float>::max();

	SPylon::UnmarkAll();
	std::queue<SPylon*> toVisit;

	for (SPylon* p : source->pylon.neighbors) {
		toVisit.push(p);
	}
	AIFloat3 P1 = target->pylon.pos;

	// breadth-first search
	while (!toVisit.empty()) {
		SPylon* q = toVisit.front();
		toVisit.pop();
		if (q == &target->pylon) {
			isFinished = true;
			costMod = MIN_COSTMOD;
			return;
		}

		float dist = P1.distance2D(q->pos) - q->range;
		if (dist < minDist) {
			minDist = dist;
			sourceHead = q;
		}

		q->SetMarked();
		for (SPylon* child : q->neighbors) {
			if (!child->Marked()) {
				toVisit.push(child);
			}
		}
	}

	if (sourceHead == nullptr) {
		sourceHead = &source->pylon;
	}
	source->head = sourceHead;

	minDist = std::numeric_limits<float>::max();
	SPylon::UnmarkAll();

	for (SPylon* p : target->pylon.neighbors) {
		toVisit.push(p);
	}
	P1 = sourceHead->pos;

	while (!toVisit.empty()) {
		SPylon* q = toVisit.front();
		toVisit.pop();
		float dist = P1.distance2D(q->pos) - q->range;
		if (dist < minDist) {
			minDist = dist;
			targetHead = q;
		}

		q->SetMarked();
		for (SPylon* child : q->neighbors) {
			if (!child->Marked()) {
				toVisit.push(child);
			}
		}
	}

	if (targetHead == nullptr) {
		targetHead = &target->pylon;
	}
	target->head = targetHead;

	const float dist = sourceHead->pos.distance2D(targetHead->pos) - sourceHead->range - targetHead->range;
	costMod = std::max(dist * invDistance, MIN_COSTMOD);
	isFinished = false;
}

void CEnergyLink::SetSource(int index)
{
	if (index != source->index) {
		std::swap(source, target);
	}
}

} // namespace circuit
