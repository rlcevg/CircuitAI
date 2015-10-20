/*
 * MoveAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/MoveAction.h"
#include "unit/UnitManager.h"
#include "terrain/PathFinder.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CMoveAction::CMoveAction(CCircuitUnit* owner)
		: IUnitAction(owner, Type::MOVE)
		, pathIterator(0)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	int squareSize = unit->GetManager()->GetCircuit()->GetPathfinder()->GetSquareSize();
	// Flying units have larger step
	increment = ((unit->GetCircuitDef()->IsAbleToFly() + 1) * DEFAULT_SLACK / squareSize + 1);
	minSqDist = squareSize * increment / 2;
	minSqDist *= minSqDist;
}

CMoveAction::CMoveAction(CCircuitUnit* owner, const F3Vec& path)
		: CMoveAction(owner)
{
	this->path = std::move(path);
}

CMoveAction::~CMoveAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CMoveAction::Update(CCircuitAI* circuit)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	Unit* u = unit->GetUnit();
	const AIFloat3& pos = u->GetPos();
	int pathMaxIndex = path.size() - 1;

	int lastStep = pathIterator;
	float sqDistToStep = pos.SqDistance2D(path[pathIterator]);
	int step = std::min(pathIterator + increment, pathMaxIndex);
	float sqNextDistToStep = pos.SqDistance2D(path[step]);
	while ((sqNextDistToStep < sqDistToStep) && (pathIterator <  pathMaxIndex)) {
		pathIterator = step;
		sqDistToStep = sqNextDistToStep;
		step = std::min(pathIterator + increment, pathMaxIndex);
		sqNextDistToStep = pos.SqDistance2D(path[step]);
	}

	if ((pathIterator == lastStep) && ((int)sqDistToStep > minSqDist)) {
		return;
	}
	pathIterator = step;
	u->MoveTo(path[step], UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);

	for (int i = 0; (step < pathMaxIndex) && (i < 2); ++i) {
		step = std::min(step + increment, pathMaxIndex);
		u->MoveTo(path[step], UNIT_COMMAND_OPTION_SHIFT_KEY, circuit->GetLastFrame() + FRAMES_PER_SEC * 60);
	}
}

void CMoveAction::SetPath(const F3Vec& path)
{
	pathIterator = 0;
	this->path = std::move(path);
}

} // namespace circuit
