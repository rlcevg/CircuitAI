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

CMoveAction::CMoveAction(CCircuitUnit* owner, int squareSize, float speed)
		: ITravelAction(owner, Type::MOVE, squareSize, speed)
{
}

CMoveAction::CMoveAction(CCircuitUnit* owner, const std::shared_ptr<F3Vec>& pPath, int squareSize, float speed)
		: ITravelAction(owner, Type::MOVE, pPath, squareSize, speed)
{
}

CMoveAction::~CMoveAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMoveAction::Update(CCircuitAI* circuit)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	int frame = circuit->GetLastFrame();

	float stepSpeed;
	int pathMaxIndex = CalcSpeedStep(frame, stepSpeed);
	if (pathMaxIndex < 0) {
		return;
	}
	int step = pathIterator;

	TRY_UNIT(circuit, unit,
		unit->GetUnit()->MoveTo((*pPath)[step], UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		unit->GetUnit()->SetWantedMaxSpeed(stepSpeed);

		for (int i = 2; (step < pathMaxIndex) && (i < 4); ++i) {
			step = std::min(step + increment, pathMaxIndex);
			unit->GetUnit()->MoveTo((*pPath)[step], UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY|UNIT_COMMAND_OPTION_SHIFT_KEY, frame + FRAMES_PER_SEC * 60 * i);
		}
	)
}

} // namespace circuit
