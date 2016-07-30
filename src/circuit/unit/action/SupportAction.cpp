/*
 * GuardAction.cpp
 *
 *  Created on: Jul 3, 2016
 *      Author: rlcevg
 */

#include "unit/action/SupportAction.h"
#include "task/fighter/SquadTask.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CSupportAction::CSupportAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::SUPPORT)
		, updCount(0)
{
	isBlocking = true;
}

CSupportAction::~CSupportAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSupportAction::Update(CCircuitAI* circuit)
{
	if (updCount++ % 2 != 0) {
		return;
	}

	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	const bool isGuard = unit->GetCircuitDef()->IsAttrMelee();
	if (isGuard && (updCount % 8 != 1)) {
		return;
	}

	CCircuitUnit* leader = static_cast<ISquadTask*>(unit->GetTask())->GetLeader();
	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = leader->GetPos(frame);
	TRY_UNIT(circuit, unit,
		if (isGuard) {
			unit->GetUnit()->Guard(leader->GetUnit());
		} else {
			unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		}
	)
}

} // namespace circuit
