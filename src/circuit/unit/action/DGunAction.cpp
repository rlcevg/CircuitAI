/*
 * DGunAction.cpp
 *
 *  Created on: Jul 29, 2015
 *      Author: rlcevg
 */

#include "unit/action/DGunAction.h"
#include "unit/EnemyUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "OOAICallback.h"
#include "Weapon.h"
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
	// NOTE: Paralyzer doesn't increase ReloadFrame beyond currentFrame, but disarmer does.
	//       Also checking disarm is more expensive (because of UnitRulesParam).
	if ((unit->GetDGun()->GetReloadFrame() > circuit->GetLastFrame()) || unit->GetUnit()->IsParalyzed() /*|| unit->IsDisarmed()*/) {
		return;
	}
	const AIFloat3& pos = unit->GetUnit()->GetPos();
	auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(pos, range));
	if (enemies.empty()) {
		return;
	}
	int canTargetCat = unit->GetCircuitDef()->GetTargetCategory();
	for (Unit* e : enemies) {
		if (e == nullptr) {
			continue;
		}
		CEnemyUnit* enemy = circuit->GetEnemyUnit(e);
		if ((enemy == nullptr) || enemy->NotInRadarAndLOS() || (enemy->GetThreat() < 0.1f)) {
			continue;
		}
		CCircuitDef* edef = enemy->GetCircuitDef();
		if ((edef != nullptr) && ((edef->GetCategory() & canTargetCat) == 0)) {
			continue;
		}

		AIFloat3 dir = enemy->GetUnit()->GetPos() - pos;
		float rayRange = dir.LengthNormalize();
		// NOTE: C API also returns rayLen
		CCircuitUnit::Id hitUID = circuit->GetDrawer()->TraceRay(pos, dir, rayRange, unit->GetUnit(), 0);

		if (hitUID == enemy->GetId()) {
			unit->ManualFire(e, circuit->GetLastFrame() + FRAMES_PER_SEC * 5);
			isBlocking = true;
			break;
		}
	}
	utils::free_clear(enemies);
}

} // namespace circuit
