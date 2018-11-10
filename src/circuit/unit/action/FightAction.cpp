/*
 * FightAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/FightAction.h"
#include "unit/CircuitUnit.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CFightAction::CFightAction(CCircuitUnit* owner, int squareSize, float speed)
		: ITravelAction(owner, Type::FIGHT, squareSize, speed)
{
}

CFightAction::CFightAction(CCircuitUnit* owner, const std::shared_ptr<F3Vec>& pPath, int squareSize, float speed)
		: ITravelAction(owner, Type::FIGHT, pPath, squareSize, speed)
{
}

CFightAction::~CFightAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CFightAction::Update(CCircuitAI* circuit)
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
		const AIFloat3& pos = (*pPath)[step];
		unit->GetUnit()->Fight(pos, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
//		unit->GetUnit()->SetWantedMaxSpeed(stepSpeed);

		constexpr short options = UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY | UNIT_COMMAND_OPTION_SHIFT_KEY;
		for (int i = 2; (step < pathMaxIndex) && (i < 4); ++i) {
			step = std::min(step + increment, pathMaxIndex);
			const AIFloat3& pos = (*pPath)[step];
			unit->GetUnit()->Fight(pos, options, frame + FRAMES_PER_SEC * 60 * i);
		}
	)
}

} // namespace circuit
