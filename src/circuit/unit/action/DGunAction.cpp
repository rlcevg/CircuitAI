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
#include "AISCommands.h"
#include "Weapon.h"

namespace circuit {

using namespace springai;

CDGunAction::CDGunAction(CCircuitUnit* owner, float range)
		: IUnitAction(owner, Type::DGUN)
		, range(range)
{
}

CDGunAction::~CDGunAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CDGunAction::Update(CCircuitAI* circuit)
{
	isBlocking = false;
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	// NOTE: Paralyzer doesn't increase ReloadFrame beyond currentFrame, but disarmer does.
	//       Also checking disarm is more expensive (because of UnitRulesParam).
	if ((unit->GetDGun()->GetReloadFrame() > circuit->GetLastFrame()) || unit->GetUnit()->IsParalyzed() /*|| unit->IsDisarmed()*/) {
		return;
	}
	Unit* u = unit->GetUnit();
	auto enemies = std::move(circuit->GetCallback()->GetEnemyUnitsIn(u->GetPos(), range));
	if (enemies.empty()) {
		return;
	}
	for (Unit* e : enemies) {
		if (e == nullptr) {
			continue;
		}
		CEnemyUnit* enemy = circuit->GetEnemyUnit(e);
		if ((enemy != nullptr) && (enemy->GetThreat() > 0.1f)) {
			u->DGun(e, UNIT_COMMAND_OPTION_ALT_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 5);
			isBlocking = true;
			break;
		}
	}
	utils::free_clear(enemies);
}

} // namespace circuit
