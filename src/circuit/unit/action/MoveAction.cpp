/*
 * MoveAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/MoveAction.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CMoveAction::CMoveAction(CCircuitUnit* owner, int squareSize, float speed)
		: ITravelAction(owner, Type::MOVE, squareSize, speed)
{
}

CMoveAction::CMoveAction(CCircuitUnit* owner, const std::shared_ptr<CPathInfo>& pPath,
		int squareSize, float speed)
		: ITravelAction(owner, Type::MOVE, pPath, squareSize, speed)
{
}

CMoveAction::~CMoveAction()
{
}

void CMoveAction::Update(CCircuitAI* circuit)
{
	if (lastFrame + FRAMES_PER_SEC > circuit->GetLastFrame()) {
		return;
	}
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	if (unit->IsJumping()) {
		return;
	}
	lastFrame = circuit->GetLastFrame();

	float stepSpeed;
	int pathMaxIndex = CalcSpeedStep(circuit, stepSpeed);
	switch (pathMaxIndex) {
		case -2:  // arrived
			TRY_UNIT(circuit, unit,
				unit->CmdMoveTo(pPath->posPath.back(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, lastFrame + FRAMES_PER_SEC * 60);
			)
			return;
		case -1: return;  // continue with current waypoints
		default: break;  // update waypoints
	}
	int step = pathIterator;

	TRY_UNIT(circuit, unit,
		constexpr short options = UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY;
		if (unit->IsAllowedToJump() && unit->IsJumpReady()) {
			const AIFloat3& startPos = unit->GetPos(lastFrame);
			const float range = unit->GetCircuitDef()->GetJumpRange();
			const float sqRange = SQUARE(range);
			for (; (step < pathMaxIndex) && (pPath->posPath[step].SqDistance2D(startPos) < sqRange); ++step);
			AIFloat3 jumpPos = pPath->posPath[std::max(0, step - 1)];
			const float sqJumpDist = jumpPos.SqDistance2D(startPos);
			bool isBadJump = sqJumpDist < SQUARE(range * 0.5f);
			if (!isBadJump) {
				isBadJump = SQUARE(range * 1.2f) < sqJumpDist;
				if (isBadJump) {
					jumpPos = startPos + (jumpPos - startPos).Normalize2D() * range;
				}
				unit->CmdJumpTo(jumpPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, lastFrame + FRAMES_PER_SEC * 60);
			}
			if (isBadJump) {
				const AIFloat3& pos = pPath->posPath[step];
				unit->CmdMoveTo(pos, options, lastFrame + FRAMES_PER_SEC * 60);
			}
		} else {
			const AIFloat3& pos = pPath->posPath[step];
			unit->CmdMoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, lastFrame + FRAMES_PER_SEC * 60);
		}
		unit->CmdWantedSpeed(stepSpeed);

		for (int i = 2; (step < pathMaxIndex) && (i < 3); ++i) {
			step = std::min(step + increment, pathMaxIndex);
			const AIFloat3& pos = pPath->posPath[step];
			unit->CmdMoveTo(pos, options, lastFrame + FRAMES_PER_SEC * 60 * i);
		}
	)
}

} // namespace circuit
