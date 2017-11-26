/*
 * DGunAction.cpp
 *
 *  Created on: Jul 29, 2015
 *      Author: rlcevg
 */

#include "unit/action/DGunAction.h"
#include "unit/CircuitUnit.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Drawer.h"

namespace circuit {

using namespace springai;

CDGunAction::CDGunAction(CCircuitUnit* owner, float range)
		: IUnitAction(owner, Type::DGUN)
		, range(range)
		, updCount(0)
{
}

CDGunAction::~CDGunAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CDGunAction::Update(CCircuitAI* circuit)
{
	if (updCount++ % 4 != 0) {
		return;
	}
	isBlocking = false;
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	const int frame = circuit->GetLastFrame();
	// NOTE: Paralyzer doesn't increase ReloadFrame beyond currentFrame, but disarmer does.
	//       Also checking disarm is more expensive (because of UnitRulesParam).
	if (!unit->IsDGunReady(frame) || unit->GetUnit()->IsParalyzed() /*|| unit->IsDisarmed(frame)*/) {
		return;
	}
	const AIFloat3& pos = unit->GetPos(frame);
	auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(pos, range));
	if (enemies.empty()) {
		return;
	}

	int canTargetCat = unit->GetCircuitDef()->GetTargetCategory();
	bool notDGunAA = !unit->GetCircuitDef()->HasDGunAA();
	CEnemyUnit* bestTarget = nullptr;
	float maxThreat = 0.f;

	for (Unit* e : enemies) {
		if (e == nullptr) {
			continue;
		}
		CEnemyUnit* enemy = circuit->GetEnemyUnit(e);
		delete e;  // replaces utils::free_clear(enemies);
		if ((enemy == nullptr) || enemy->NotInRadarAndLOS() || (enemy->GetThreat() < THREAT_MIN)) {
			continue;
		}
		CCircuitDef* edef = enemy->GetCircuitDef();
		if ((edef == nullptr) || ((edef->GetCategory() & canTargetCat) == 0) || (edef->IsAbleToFly() && notDGunAA)) {
			continue;
		}

		AIFloat3 dir = enemy->GetUnit()->GetPos() - pos;
		float rayRange = dir.LengthNormalize();
		// NOTE: TraceRay check is mostly to ensure shot won't go into terrain.
		//       Doesn't properly work with standoff weapons.
		//       C API also returns rayLen.
		ICoreUnit::Id hitUID = circuit->GetDrawer()->TraceRay(pos, dir, rayRange, unit->GetUnit(), 0);
		if (hitUID != enemy->GetId()) {
			continue;
		}

		const float defThreat = edef->GetPower();
		if (maxThreat < defThreat) {
			maxThreat = defThreat;
			bestTarget = enemy;
		}
	}

	if (bestTarget != nullptr) {
		unit->ManualFire(bestTarget, frame + FRAMES_PER_SEC * 5);
		isBlocking = true;
	}
}

} // namespace circuit
