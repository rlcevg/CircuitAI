/*
 * MoveAction.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: rlcevg
 */

#include "unit/action/MoveAction.h"
#include "unit/UnitManager.h"
#include "terrain/TerrainManager.h"
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
	int squareSize = unit->GetManager()->GetCircuit()->GetTerrainManager()->GetConvertStoP();
	increment = ((SQUARE_SIZE * THREAT_RES) / squareSize + 1);
	minSqDist = squareSize * increment;
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
	u->MoveTo(path[pathIterator], UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, FRAMES_PER_SEC * 60);

	if (pathIterator >= pathMaxIndex) {
		return;
	}
	step = std::min(pathIterator + increment * 2, pathMaxIndex);
	u->MoveTo(path[step], UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 120);

	if (step >= pathMaxIndex) {
		return;
	}
	step = std::min(pathIterator + increment * 4, pathMaxIndex);
	u->MoveTo(path[step], UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 180);
}

void CMoveAction::SetPath(const F3Vec& path)
{
	pathIterator = 0;
	this->path = std::move(path);
}

} // namespace circuit
