/*
 * GuardAction.cpp
 *
 *  Created on: Jul 3, 2016
 *      Author: rlcevg
 */

#include "unit/action/SupportAction.h"
#include "unit/CircuitUnit.h"
#include "task/fighter/SquadTask.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CSupportAction::CSupportAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::SUPPORT)
		, updCount(0)
{
	isBlocking = true;
	CCircuitDef* cdef = owner->GetCircuitDef();
	isLowUpdate = cdef->IsAttrMelee() || (cdef->GetReloadTime() >= FRAMES_PER_SEC * 5);
}

CSupportAction::~CSupportAction()
{
}

void CSupportAction::Update(CCircuitAI* circuit)
{
	if ((updCount++ % 2 != 0) ||
		(isLowUpdate && (updCount % 4 != 1)))
	{
		return;
	}

	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	CCircuitUnit* leader = static_cast<ISquadTask*>(unit->GetTask())->GetLeader();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = leader->GetPos(frame);
	if (pos.SqDistance2D(unit->GetPos(frame)) < SQUARE(SQUARE_SIZE * 8)) {
		return;  // stop pushing
	}
	TRY_UNIT(circuit, unit,
		if (unit->GetCircuitDef()->IsAttrMelee()) {
			unit->GetUnit()->Guard(leader->GetUnit());
		} else {
			unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		}
	)
}

} // namespace circuit
