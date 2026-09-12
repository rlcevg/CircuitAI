/*
 * FightAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/FightAction.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CFightAction::CFightAction(CCircuitUnit* owner, int squareSize, float speed)
		: ITravelAction(owner, Type::FIGHT, squareSize, speed)
{
}

CFightAction::CFightAction(CCircuitUnit* owner, const std::shared_ptr<CPathInfo>& pPath,
		int squareSize, float speed)
		: ITravelAction(owner, Type::FIGHT, pPath, squareSize, speed)
{
}

CFightAction::~CFightAction()
{
}

void CFightAction::Update(CCircuitAI* circuit)
{
	if (lastFrame + FRAMES_PER_SEC * 2 > circuit->GetLastFrame()) {
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
				unit->CmdFightTo(pPath->posPath.back(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, lastFrame + FRAMES_PER_SEC * 60);
			)
			return;
		case -1: return;  // continue with current waypoints
		default: break;  // update waypoints
	}
	int step = pathIterator;

	TRY_UNIT(circuit, unit,
		constexpr short options = UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY;
		const AIFloat3& startPos = unit->GetPos(lastFrame);
		AIFloat3 nextPos;
		if (unit->IsAllowedToJump() && unit->IsJumpReady()) {
			const float range = unit->GetCircuitDef()->GetJumpRange();
			const float sqRange = SQUARE(range);
			for (; (step < pathMaxIndex) && (pPath->posPath[step].SqDistance2D(startPos) < sqRange); ++step);
			nextPos = pPath->posPath[std::max(0, step - 1)];
			const float sqJumpDist = nextPos.SqDistance2D(startPos);
			bool isBadJump = sqJumpDist < SQUARE(range * 0.5f);
			if (!isBadJump) {
				isBadJump = SQUARE(range * 1.2f) < sqJumpDist;
				if (isBadJump) {
					nextPos = startPos + (nextPos - startPos).Normalize2D() * range;
				}
				unit->CmdJumpTo(nextPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, lastFrame + FRAMES_PER_SEC * 300);
			}
			if (isBadJump) {
				nextPos = pPath->posPath[step];
				unit->CmdFightTo(nextPos, options, lastFrame + FRAMES_PER_SEC * 300);
			}
		} else {
			nextPos = pPath->posPath[step];
			unit->CmdFightTo(nextPos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, lastFrame + FRAMES_PER_SEC * 300);
		}
		unit->CmdWantedSpeed(stepSpeed);

		if (!geom::is_in_range(startPos, nextPos, DUP_CMD_DIST)) {
			return;
		}
		for (int i = 2; (step < pathMaxIndex) && (i < 3); ++i) {
			step = std::min(step + increment, pathMaxIndex);
			const AIFloat3& pos = pPath->posPath[step];
			unit->CmdFightTo(pos, options, lastFrame + FRAMES_PER_SEC * 60 * i);
		}
	)
}

} // namespace circuit
