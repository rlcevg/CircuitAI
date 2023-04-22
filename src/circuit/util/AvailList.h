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
#include <unordered_map>
#include <algorithm>

namespace circuit {

template <typename T, typename S = std::function<float (CCircuitDef*, T&)>>
class CAvailList {
public:
	struct SAvailInfo {
		CCircuitDef* cdef;
		float score;
		bool operator==(const CCircuitDef* d) const { return cdef == d; }
		T data;
	};

	CAvailList() {}
	~CAvailList() {}

	void Init(const std::vector<CCircuitDef*>& builders, S func);

	void AddDef(CCircuitDef* cdef) { all.insert(cdef); }

	void AddDefs(const std::set<CCircuitDef*>& buildDefs);
	void RemoveDefs(const std::set<CCircuitDef*>& buildDefs);

	const std::set<CCircuitDef*>& GetAll() const { return all; }
	bool IsAvail(CCircuitDef* buildDef) const { return avail.find(buildDef) != avail.end(); }
	bool HasAvail() const { return !infos.empty(); }
	const T* GetAvailInfo(const CCircuitDef* cdef) const;
	CCircuitDef* GetFirstDef() const { return infos.front().cdef; }
	template <typename F> CCircuitDef* GetBestDef(F filterFunc) const;
	template <typename F> CCircuitDef* GetWorstDef(F filterFunc) const;
	const std::vector<SAvailInfo>& GetInfos() const { return infos; }

	const std::vector<CCircuitDef*>& GetBuildDefs(CCircuitDef* conDef) {
		return builderInfos[conDef->GetId()];
	}

private:
	std::set<CCircuitDef*> all;
	std::set<CCircuitDef*> avail;
	std::vector<SAvailInfo> infos;  // sorted high-score first

	S scoreFunc;
	std::unordered_map<CCircuitDef::Id, std::vector<CCircuitDef*>> builderInfos;
};

template <typename T, typename S>
void CAvailList<T, S>::Init(const std::vector<CCircuitDef*>& builders, S func)
{
	scoreFunc = func;

	std::vector<SAvailInfo> allInfos;
	for (CCircuitDef* cdef : all) {
		SAvailInfo info;
		info.cdef = cdef;
		info.score = scoreFunc(cdef, info.data);
		allInfos.push_back(info);
	}
	// High-tech first
	auto compare = [](const SAvailInfo& e1, const SAvailInfo& e2) {
		return e1.score > e2.score;
	};
	std::sort(allInfos.begin(), allInfos.end(), compare);

	for (CCircuitDef const* bdef : builders) {
		for (const SAvailInfo& info : allInfos) {
			if (bdef->CanBuild(info.cdef)) {
				builderInfos[bdef->GetId()].push_back(info.cdef);
			}
		}
	}
}

template <typename T, typename S>
void CAvailList<T, S>::AddDefs(const std::set<CCircuitDef*>& buildDefs)
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

template <typename T, typename S>
void CAvailList<T, S>::RemoveDefs(const std::set<CCircuitDef*>& buildDefs)
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

template <typename T, typename S>
const T* CAvailList<T, S>::GetAvailInfo(const CCircuitDef* cdef) const
{
	auto it = std::find(infos.begin(), infos.end(), cdef);
	return (it != infos.end()) ? &it->data : nullptr;
}

template <typename T, typename S>
template <typename F>
CCircuitDef* CAvailList<T, S>::GetBestDef(F filterFunc) const
{
	for (auto& info : infos) {
		if (filterFunc(info.cdef, info.data)) {
			return info.cdef;
		}
	}
	return nullptr;
}

template <typename T, typename S>
template <typename F>
CCircuitDef* CAvailList<T, S>::GetWorstDef(F filterFunc) const
{
	auto it = infos.rbegin();
	while (it != infos.rend()) {
		if (filterFunc(it->cdef, it->data)) {
			return it->cdef;
		}
		++it;
	}
	return nullptr;
}

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_AVAILLIST_H_
