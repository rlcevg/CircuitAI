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
#include "spring/SpringMap.h"

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
	if (!unit->IsDGunReady(frame, circuit->GetEconomyManager()->GetEnergyCur() * 0.9f)
		|| unit->GetUnit()->IsParalyzed()/* || unit->IsDisarmed(frame)*/)
	{
		return;
	}
	const AIFloat3& pos = unit->GetPos(frame);
	auto& enemies = circuit->GetCallback()->GetEnemyUnitIdsIn(pos, range);
	if (enemies.empty()) {
		return;
	}

	CMap* map = circuit->GetMap();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const int canTargetCat = cdef->GetTargetCategoryDGun();
	const bool isRoleComm = cdef->IsRoleComm();
	const bool IsInWater = cdef->IsInWater(map->GetElevationAt(pos.x, pos.z), pos.y);
	const bool isLowTraj = !unit->IsDGunHigh();
	CEnemyInfo* bestTarget = nullptr;
	float maxThreat = 0.f;

	for (int eId : enemies) {
		CEnemyInfo* enemy = circuit->GetEnemyInfo(eId);
		if ((enemy == nullptr) || enemy->NotInRadarAndLOS() || (enemy->GetInfluence() < THREAT_MIN)) {
			continue;
		}
		CCircuitDef* edef = enemy->GetCircuitDef();
		if ((edef == nullptr)
			|| ((edef->GetCategory() & canTargetCat) == 0)
			|| (isRoleComm && edef->IsRoleComm()))  // NOTE: BAR, comm kamikaze
		{
			continue;
		}

		const AIFloat3& ePos = enemy->GetPos();
		const float elevation = map->GetElevationAt(ePos.x, ePos.z);
		if (edef->IsAbleToFly() && !(IsInWater ? cdef->HasSubToAirDGun() : cdef->HasSurfToAirDGun())) {  // notAA
			continue;
		}
		if (edef->IsInWater(elevation, ePos.y)) {
			if (!(IsInWater ? cdef->HasSubToWaterDGun() : cdef->HasSurfToWaterDGun())) {  // notAW
				continue;
			}
		} else {
			if (!(IsInWater ? cdef->HasSubToLandDGun() : cdef->HasSurfToLandDGun())) {  // notAL
				continue;
			}
		}

		if (isLowTraj) {
			AIFloat3 dir = enemy->GetPos() - pos;
			float rayRange = dir.LengthNormalize();
			// NOTE: TraceRay check is mostly to ensure shot won't go into terrain.
			//       Doesn't properly work with standoff weapons.
			//       C API also returns rayLen.
			ICoreUnit::Id hitUID = circuit->GetDrawer()->TraceRay(pos, dir, rayRange, unit->GetUnit(), 0);
			if (hitUID != enemy->GetId()) {
				continue;
			}
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
