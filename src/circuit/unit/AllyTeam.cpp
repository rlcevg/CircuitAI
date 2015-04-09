/*
 * AllyTeam.cpp
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#include "unit/AllyTeam.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "static/GameAttribute.h"
#include "static/TerrainData.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AIFloat3.h"
#include "OOAICallback.h"
#include "UnitDef.h"

namespace circuit {

using namespace springai;

bool CAllyTeam::SBox::ContainsPoint(const AIFloat3& point) const
{
	return (point.x >= left) && (point.x <= right) &&
		   (point.z >= top) && (point.z <= bottom);
}

CAllyTeam::CAllyTeam(const std::vector<int>& tids, const SBox& sb) :
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
		UnitDef* def = defsById[unitDef->GetUnitDefId()];
		CCircuitUnit* unit = new CCircuitUnit(u, def, circuitDefs[def]);
		delete unitDef;
		friendlyUnits[unitId] = unit;
	}
	lastUpdate = frame;
}

CAllyTeam::UnitDefs* CAllyTeam::GetDefsByName()
{
	return &defsByName;
}

CAllyTeam::IdUnitDefs* CAllyTeam::GetDefsById()
{
	return &defsById;
}

CAllyTeam::CircuitDefs* CAllyTeam::GetCircuitDefs()
{
	return &circuitDefs;
}

void CAllyTeam::Init(CCircuitAI* circuit)
{
	if (initCount++ > 0) {
		return;
	}

	const std::vector<UnitDef*>& unitDefs = circuit->GetCallback()->GetUnitDefs();
	for (auto def : unitDefs) {
		defsByName[def->GetName()] = def;
		defsById[def->GetUnitDefId()] = def;
	}

	CTerrainData& terrainData = circuit->GetGameAttribute()->GetTerrainData();
	if (!terrainData.IsInitialized()) {
		terrainData.Init(circuit);
	}
	for (auto& kv : defsById) {
		std::vector<UnitDef*> options = kv.second->GetBuildOptions();
		std::unordered_set<UnitDef*> opts;
		for (auto buildDef : options) {
			// if it breaks with defsById[] then something really wrong is going on
			opts.insert(defsById[buildDef->GetUnitDefId()]);
		}
		UnitDef* ud = kv.second;
		CCircuitDef* cdef = new CCircuitDef(opts);
		circuitDefs[ud] = cdef;
		utils::free_clear(options);

		if (ud->IsAbleToFly()) {
		} else if (ud->GetSpeed() == 0 ) {  // for immobile units
			cdef->SetImmobileTypeId(terrainData.udImmobileType[ud->GetUnitDefId()]);
			// TODO: SetMobileType for factories (like RAI does)
		} else {  // for mobile units
			cdef->SetMobileTypeId(terrainData.udMobileType[ud->GetUnitDefId()]);
		}
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

	for (auto& kv : circuitDefs) {
		delete kv.second;
	}
	circuitDefs.clear();

	for (auto& kv : defsByName) {
		delete kv.second;
	}
	defsByName.clear();
	defsById.clear();
}

} // namespace circuit
