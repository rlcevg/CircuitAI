/*
 * AvailList.h
 *
 *  Created on: May 25, 2021
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_AVAILLIST_H_
#define SRC_CIRCUIT_UTIL_AVAILLIST_H_

#include "unit/CircuitDef.h"

#include <set>
#include <vector>
#include <algorithm>

namespace circuit {

template <typename T>
class CAvailList {
public:
	struct SAvailInfo {
		CCircuitDef* cdef;
		float score;
		bool operator==(const CCircuitDef* d) { return cdef == d; }
		T data;
	};

	CAvailList() {}
	~CAvailList() {}

	void AddDef(CCircuitDef* cdef) { all.insert(cdef); }

	template <typename S>  // using S = std::function<float (CCircuitDef*, T&)>;
	void AddDefs(const std::set<CCircuitDef*>& buildDefs, S scoreFunc);
	void RemoveDefs(const std::set<CCircuitDef*>& buildDefs);

	const std::set<CCircuitDef*>& GetAll() const { return all; }
	bool IsAvail(CCircuitDef* buildDef) const { return avail.find(buildDef) != avail.end(); }
	const std::vector<SAvailInfo>& GetInfos() const { return infos; }

private:
	std::set<CCircuitDef*> all;
	std::set<CCircuitDef*> avail;

public:
	std::vector<SAvailInfo> infos;  // sorted high-score first
};

template <typename T>
template <typename S>
void CAvailList<T>::AddDefs(const std::set<CCircuitDef*>& buildDefs, S scoreFunc)
{
	std::set<CCircuitDef*> newDefs;
	std::set_intersection(all.begin(), all.end(),
						  buildDefs.begin(), buildDefs.end(),
						  std::inserter(newDefs, newDefs.begin()));
	if (newDefs.empty()) {
		return;
	}
	std::set<CCircuitDef*> diffDefs;
	std::set_difference(newDefs.begin(), newDefs.end(),
						avail.begin(), avail.end(),
						std::inserter(diffDefs, diffDefs.begin()));
	if (diffDefs.empty()) {
		return;
	}
	avail.insert(diffDefs.begin(), diffDefs.end());

	for (CCircuitDef* cdef : diffDefs) {
		SAvailInfo info;
		info.cdef = cdef;
		info.score = scoreFunc(cdef, info.data);
		infos.push_back(info);
	}

	// High-tech first
	auto compare = [](const SAvailInfo& e1, const SAvailInfo& e2) {
		return e1.score > e2.score;
	};
	std::sort(infos.begin(), infos.end(), compare);
}

template <typename T>
void CAvailList<T>::RemoveDefs(const std::set<CCircuitDef*>& buildDefs)
{
	std::set<CCircuitDef*> oldDefs;
	std::set_intersection(all.begin(), all.end(),
						  buildDefs.begin(), buildDefs.end(),
						  std::inserter(oldDefs, oldDefs.begin()));
	if (oldDefs.empty()) {
		return;
	}
	std::set<CCircuitDef*> diffDefs;
	std::set_difference(avail.begin(), avail.end(),
						oldDefs.begin(), oldDefs.end(),
						std::inserter(diffDefs, diffDefs.begin()));
	std::swap(avail, diffDefs);

	auto it = infos.end();
	while (it > infos.begin()) {
		--it;
		auto search = oldDefs.find(it->cdef);
		if (search != oldDefs.end()) {
			it = infos.erase(it);
		}
	}
}

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_AVAILLIST_H_
