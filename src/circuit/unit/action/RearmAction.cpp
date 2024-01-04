/*
 * RearmAction.cpp
 *
 *  Created on: Dec 31, 2023
 *      Author: rlcevg
 */

#include "unit/action/RearmAction.h"
#include "unit/CircuitUnit.h"
#include "task/UnitTask.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

CRearmAction::CRearmAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::REARM)
		, updCount(0)
{
}

CRearmAction::~CRearmAction()
{
}

void CRearmAction::Update(CCircuitAI* circuit)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	const int frame = circuit->GetLastFrame();
	isBlocking = !unit->IsWeaponReady(frame);
	if (!isBlocking) {
		updCount = 0;
		return;
	}

	if (updCount++ % 32 == 0) {
		unit->SetTaskFrame(frame);  // avoid UnitIdle on find_pad
		TRY_UNIT(circuit, unit,
			unit->CmdFindPad(frame + FRAMES_PER_SEC * 60);
		)
	}
}

void CRearmAction::OnStart()
{
	IAction::OnStart();
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	unit->GetTask()->OnRearmStart(unit);
}

} // namespace circuit
