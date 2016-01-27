/*
 * FightAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/FightAction.h"
#include "unit/UnitManager.h"
#include "terrain/PathFinder.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CFightAction::CFightAction(CCircuitUnit* owner, float speed)
		: IUnitAction(owner, Type::MOVE)
		, speed(speed)
		, pathIterator(0)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	int squareSize = unit->GetManager()->GetCircuit()->GetPathfinder()->GetSquareSize();
	int incMod;
	if (unit->GetCircuitDef()->IsPlane()) {
		incMod = 5;
	} else if (unit->GetCircuitDef()->IsAbleToFly()) {
		incMod = 3;
	} else if (unit->GetCircuitDef()->IsTurnLarge()) {
		incMod = 2;
	} else {
		incMod = 1;
	}
	increment = incMod * DEFAULT_SLACK / squareSize + 1;
	minSqDist = squareSize * increment / 2;
	minSqDist *= minSqDist;
}

CFightAction::CFightAction(CCircuitUnit* owner, const std::shared_ptr<F3Vec>& pPath, float speed)
		: CFightAction(owner, speed)
{
	this->pPath = pPath;
}

CFightAction::~CFightAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CFightAction::Update(CCircuitAI* circuit)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	int pathMaxIndex = pPath->size() - 1;

	int lastStep = pathIterator;
	float sqDistToStep = pos.SqDistance2D((*pPath)[pathIterator]);
	int step = std::min(pathIterator + increment, pathMaxIndex);
	float sqNextDistToStep = pos.SqDistance2D((*pPath)[step]);
	while ((sqNextDistToStep < sqDistToStep) && (pathIterator <  pathMaxIndex)) {
		pathIterator = step;
		sqDistToStep = sqNextDistToStep;
		step = std::min(pathIterator + increment, pathMaxIndex);
		sqNextDistToStep = pos.SqDistance2D((*pPath)[step]);
	}

	if ((pathIterator == lastStep) && ((int)sqDistToStep > minSqDist)) {
		return;
	}
	pathIterator = step;
	unit->GetUnit()->Fight((*pPath)[step], UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	unit->GetUnit()->SetWantedMaxSpeed(speed);

	for (int i = 2; (step < pathMaxIndex) && (i < 4); ++i) {
		step = std::min(step + increment, pathMaxIndex);
		unit->GetUnit()->Fight((*pPath)[step], UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY|UNIT_COMMAND_OPTION_SHIFT_KEY, frame + FRAMES_PER_SEC * 60 * i);
	}
}

void CFightAction::SetPath(const std::shared_ptr<F3Vec>& pPath, float speed)
{
	pathIterator = 0;
	this->pPath = pPath;
	this->speed = speed;
}

} // namespace circuit
