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
	int sqDistToStep = pos.SqDistance2D(path[pathIterator]);
	int squareSize = circuit->GetTerrainManager()->GetConvertStoP();
	int maxSqDist = squareSize * squareSize * 4;
	const int increment = 3 * (SQUARE_SIZE * THREAT_RES) / squareSize;

	if (sqDistToStep < maxSqDist) {
		pathIterator = std::min(pathIterator + increment, pathMaxIndex);
		u->MoveTo(path[pathIterator], UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, FRAMES_PER_SEC * 300);

		if (pathIterator < pathMaxIndex) {
			int nextStep = std::min(pathIterator + increment * 2, pathMaxIndex);
			u->MoveTo(path[nextStep], UNIT_COMMAND_OPTION_SHIFT_KEY, FRAMES_PER_SEC * 300);
		}
	}
}

void CMoveAction::SetPath(const F3Vec& path)
{
	pathIterator = 0;
	this->path = std::move(path);
}

} // namespace circuit
