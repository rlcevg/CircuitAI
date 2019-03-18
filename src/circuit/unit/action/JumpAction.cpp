/*
 * JumpAction.cpp
 *
 *  Created on: Jan 26, 2016
 *      Author: rlcevg
 */

#include "unit/action/JumpAction.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CJumpAction::CJumpAction(CCircuitUnit* owner, int squareSize, float speed)
		: ITravelAction(owner, Type::JUMP, squareSize, speed)
{
}

CJumpAction::CJumpAction(CCircuitUnit* owner, const std::shared_ptr<F3Vec>& pPath, int squareSize, float speed)
		: ITravelAction(owner, Type::JUMP, pPath, squareSize, speed)
{
}

CJumpAction::~CJumpAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CJumpAction::Update(CCircuitAI* circuit)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	if (unit->IsJumping()) {
		return;
	}
	const int frame = circuit->GetLastFrame();

	float stepSpeed;
	int pathMaxIndex = CalcSpeedStep(frame, stepSpeed);
	if (pathMaxIndex < 0) {
		return;
	}
	int step = pathIterator;

	TRY_UNIT(circuit, unit,
		if (unit->IsJumpReady()) {
			AIFloat3 startPos = unit->GetPos(frame);
			const float sqRange = SQUARE(unit->GetCircuitDef()->GetJumpRange());
			for (; (step < pathMaxIndex) && ((*pPath)[step].SqDistance2D(startPos) < sqRange); ++step);
			const AIFloat3& jumpPos = (*pPath)[std::max(0, step - 1)];

			unit->GetUnit()->ExecuteCustomCommand(CMD_JUMP,
												  {jumpPos.x, jumpPos.y, jumpPos.z},
												  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY,
												  frame + FRAMES_PER_SEC * 60);
		} else {
			const AIFloat3& pos = (*pPath)[step];
//			unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
			unit->GetUnit()->ExecuteCustomCommand(CMD_RAW_MOVE,
												  {pos.x, pos.y, pos.z},
												  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY,
												  frame + FRAMES_PER_SEC * 60);
		}
		unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {stepSpeed});

		constexpr short options = UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY;
		for (int i = 2; (step < pathMaxIndex) && (i < 4); ++i) {
			step = std::min(step + increment, pathMaxIndex);
			const AIFloat3& pos = (*pPath)[step];
//			unit->GetUnit()->MoveTo(pos, options, frame + FRAMES_PER_SEC * 60 * i);
			unit->GetUnit()->ExecuteCustomCommand(CMD_RAW_MOVE,
												  {pos.x, pos.y, pos.z},
												  options,
												  frame + FRAMES_PER_SEC * 60);
		}
	)
}

} // namespace circuit
