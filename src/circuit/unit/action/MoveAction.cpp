/*
 * MoveAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CMoveAction::CMoveAction(CCircuitUnit* owner, const AIFloat3& pos, MoveType mt) :
		IUnitAction(owner, Type::MOVE),
		position(pos),
		moveType(mt)
{
}

CMoveAction::~CMoveAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMoveAction::Update(CCircuitAI* circuit)
{

}

void CMoveAction::OnStart()
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	unit->GetUnit()->MoveTo(position, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 1);
}

void CMoveAction::OnEnd()
{
}

} // namespace circuit
