/*
 * AntiCapAction.cpp
 *
 *  Created on: Mar 5, 2021
 *      Author: rlcevg
 */

#include "unit/action/AntiCapAction.h"
#include "unit/CircuitUnit.h"
#include "setup/SetupManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CAntiCapAction::CAntiCapAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::ANTI_CAP)
{
}

CAntiCapAction::~CAntiCapAction()
{
}

void CAntiCapAction::Update(CCircuitAI* circuit)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	if (unit->IsInSelfD()) {
		return;
	}

	const float captureProgress = unit->GetUnit()->GetCaptureProgress();
	if (captureProgress < 1e-3f) {
		return;
	}
	const float health = unit->GetUnit()->GetHealth();
	const float maxHealth = unit->GetUnit()->GetMaxHealth();
	const float captureSpeed = circuit->GetSetupManager()->GetCommChoice()->GetCaptureSpeed();
	// @see rts/Sim/Units/UnitTypes/Builder.cpp:487-500 (captureMagicNumber)
	const float magic = (150.0f + (unit->GetCircuitDef()->GetBuildTime() / captureSpeed) * (health + maxHealth) / maxHealth * 0.4f);
	const float stepToDestr = (unit->GetCircuitDef()->GetSelfDCountdown() + 1) * FRAMES_PER_SEC / magic;
	if (captureProgress + stepToDestr > 1.f) {
		unit->CmdSelfD(true);
	}
}

} // namespace circuit
