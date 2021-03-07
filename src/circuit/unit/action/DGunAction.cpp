/*
 * DGunAction.cpp
 *
 *  Created on: Jul 29, 2015
 *      Author: rlcevg
 */

#include "unit/action/DGunAction.h"
#include "unit/enemy/EnemyUnit.h"
#include "unit/CircuitUnit.h"
#include "module/EconomyManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

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
	if (!unit->IsDGunReady(frame, circuit->GetEconomyManager()->GetEnergyCur()) || unit->GetUnit()->IsParalyzed() /*|| unit->IsDisarmed(frame)*/) {
		return;
	}
	const AIFloat3& pos = unit->GetPos(frame);
	auto enemies = circuit->GetCallback()->GetEnemyUnitIdsIn(pos, range);
	if (enemies.empty()) {
		return;
	}

	const int canTargetCat = unit->GetCircuitDef()->GetTargetCategory();
	const bool notDGunAA = !unit->GetCircuitDef()->HasDGunAA();
	const bool isRoleComm = unit->GetCircuitDef()->IsRoleComm();
	CCircuitDef::RoleT role = unit->GetCircuitDef()->GetMainRole();
	CEnemyInfo* bestTarget = nullptr;
	float maxThreat = 0.f;

	for (int eId : enemies) {
		if (eId == -1) {
			continue;
		}
		CEnemyInfo* enemy = circuit->GetEnemyInfo(eId);
		if ((enemy == nullptr) || enemy->NotInRadarAndLOS() || (enemy->GetThreat(role) < THREAT_MIN)) {
			continue;
		}
		CCircuitDef* edef = enemy->GetCircuitDef();
		if ((edef == nullptr) || ((edef->GetCategory() & canTargetCat) == 0) || (edef->IsAbleToFly() && notDGunAA)
			|| (isRoleComm && edef->IsRoleComm()))  // FIXME: comm kamikaze
		{
			continue;
		}

		AIFloat3 dir = enemy->GetPos() - pos;
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
		unit->ClearTarget();
		isBlocking = true;
	}
}

} // namespace circuit
