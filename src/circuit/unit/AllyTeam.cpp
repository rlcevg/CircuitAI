/*
 * AllyTeam.cpp
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#include "unit/AllyTeam.h"
#include "static/GameAttribute.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AIFloat3.h"
#include "OOAICallback.h"

namespace circuit {

using namespace springai;

bool CAllyTeam::SBox::ContainsPoint(const AIFloat3& point) const
{
	return (point.x >= left) && (point.x <= right) &&
		   (point.z >= top) && (point.z <= bottom);
}

CAllyTeam::CAllyTeam(const std::vector<Id>& tids, const SBox& sb) :
		teamIds(tids),
		startBox(sb),
		lastUpdate(-1),
		initCount(0)
{
}

CAllyTeam::~CAllyTeam()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	if (initCount > 0) {
		initCount = 1;
		Release();
	}
}

int CAllyTeam::GetSize() const
{
	return teamIds.size();
}

const CAllyTeam::SBox& CAllyTeam::GetStartBox() const
{
	return startBox;
}

void CAllyTeam::UpdateUnits(int frame, springai::OOAICallback* callback)
{
	if (lastUpdate >= frame) {
		return;
	}

	for (auto& kv : friendlyUnits) {
		delete kv.second;
	}
	friendlyUnits.clear();
	const std::vector<Unit*>& units = callback->GetFriendlyUnits();
	for (auto u : units) {
		// FIXME: Why engine returns vector with some nullptrs?
		// TODO: Check every GetEnemy/FriendlyUnits for nullptr
		if (u == nullptr) {
			continue;
		}
		int unitId = u->GetUnitId();
		UnitDef* unitDef = u->GetDef();
		CCircuitUnit* unit = new CCircuitUnit(u, defsById[unitDef->GetUnitDefId()]);
		delete unitDef;
		friendlyUnits[unitId] = unit;
	}
	lastUpdate = frame;
}

CAllyTeam::CircuitDefs* CAllyTeam::GetDefsById()
{
	return &defsById;
}

CAllyTeam::NCircuitDefs* CAllyTeam::GetDefsByName()
{
	return &defsByName;
}

void CAllyTeam::Init(CCircuitAI* circuit)
{
	if (initCount++ > 0) {
		return;
	}

	CTerrainData& terrainData = circuit->GetGameAttribute()->GetTerrainData();
	const std::vector<UnitDef*>& unitDefs = circuit->GetCallback()->GetUnitDefs();
	for (auto ud : unitDefs) {
		auto options = std::move(ud->GetBuildOptions());
		std::unordered_set<CCircuitDef::Id> opts;
		for (auto buildDef : options) {
			opts.insert(buildDef->GetUnitDefId());
			delete buildDef;
		}
		CCircuitDef* cdef = new CCircuitDef(ud, opts);

		if (ud->IsAbleToFly()) {
		} else if (ud->GetSpeed() == 0 ) {  // for immobile units
			cdef->SetImmobileId(terrainData.udImmobileType[cdef->GetId()]);
			// TODO: SetMobileType for factories (like RAI does)
		} else {  // for mobile units
			cdef->SetMobileId(terrainData.udMobileType[cdef->GetId()]);
		}

		defsByName[ud->GetName()] = cdef;
		defsById[cdef->GetId()] = cdef;
	}
}

void CAllyTeam::Release()
{
	if (--initCount > 0) {
		return;
	}

	for (auto& kv : friendlyUnits) {
		delete kv.second;
	}
	friendlyUnits.clear();

	for (auto& kv : enemyUnits) {
		delete kv.second;
	}
	enemyUnits.clear();

	for (auto& kv : defsById) {
		delete kv.second;
	}
	defsById.clear();
	defsByName.clear();
}

} // namespace circuit
