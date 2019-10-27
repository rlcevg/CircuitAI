/*
 * MoveAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/MoveAction.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CMoveAction::CMoveAction(CCircuitUnit* owner, int squareSize, float speed)
		: ITravelAction(owner, Type::MOVE, squareSize, speed)
{
}

CMoveAction::CMoveAction(CCircuitUnit* owner, const std::shared_ptr<PathInfo>& pPath,
		int squareSize, float speed)
		: ITravelAction(owner, Type::MOVE, pPath, squareSize, speed)
{
}

CMoveAction::~CMoveAction()
{
}

void CMoveAction::Update(CCircuitAI* circuit)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	const int frame = circuit->GetLastFrame();

	float stepSpeed;
	int pathMaxIndex = CalcSpeedStep(frame, stepSpeed);
	if (pathMaxIndex < 0) {
		return;
	}
	int step = pathIterator;

	TRY_UNIT(circuit, unit,
		const AIFloat3& pos = pPath->posPath[step];
//		unit->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		unit->GetUnit()->ExecuteCustomCommand(CMD_RAW_MOVE,
											  {pos.x, pos.y, pos.z},
											  UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY,
											  frame + FRAMES_PER_SEC * 60);
		unit->GetUnit()->ExecuteCustomCommand(CMD_WANTED_SPEED, {stepSpeed});

		constexpr short options = UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY;
		for (int i = 2; (step < pathMaxIndex) && (i < 4); ++i) {
			step = std::min(step + increment, pathMaxIndex);
			const AIFloat3& pos = pPath->posPath[step];
//			unit->GetUnit()->MoveTo(pos, options, frame + FRAMES_PER_SEC * 60 * i);
			unit->GetUnit()->ExecuteCustomCommand(CMD_RAW_MOVE,
												  {pos.x, pos.y, pos.z},
												  options,
												  frame + FRAMES_PER_SEC * 60 * i);
		}
	)
}

} // namespace circuit
