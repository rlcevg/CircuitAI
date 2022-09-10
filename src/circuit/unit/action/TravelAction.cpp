/*
 * TravelAction.cpp
 *
 *  Created on: Feb 16, 2016
 *      Author: rlcevg
 */

#include "unit/action/TravelAction.h"
#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"
#include "task/UnitTask.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"

namespace circuit {

using namespace springai;

ITravelAction::ITravelAction(CCircuitUnit* owner, Type type, int squareSize, float speed)
		: IUnitAction(owner, type)
		, speed(speed)
		, pathIterator(0)
		, lastSector(-1)
		, lastFrame(-1)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	CCircuitDef* cdef = unit->GetCircuitDef();
	int size = std::max(cdef->GetDef()->GetXSize(), cdef->GetDef()->GetZSize());
	int incMod = std::max(size / 4, 1);
	if (cdef->IsPlane()) {
		incMod *= 8;
	} else if (cdef->IsAbleToFly()) {
		incMod *= 3;
	} else if (cdef->IsTurnLarge()) {
		incMod *= 2;
	}
	increment = incMod * DEFAULT_SLACK / squareSize + 1;
	minSqDist = squareSize * increment;  // / 2;
	minSqDist *= minSqDist;
}

ITravelAction::ITravelAction(CCircuitUnit* owner, Type type, const std::shared_ptr<CPathInfo>& pPath,
		int squareSize, float speed)
		: ITravelAction(owner, type, squareSize, speed)
{
	this->pPath = pPath;
}

ITravelAction::~ITravelAction()
{
}

void ITravelAction::OnEnd()
{
	IAction::OnEnd();
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	unit->GetTask()->OnTravelEnd(unit);  // WARNING: do not clear/delete unit actions
}

void ITravelAction::SetPath(const std::shared_ptr<CPathInfo>& pPath, float speed)
{
	// NOTE: pPath can be null. Caller uses StateWait() after this call in such cases
	pathIterator = 0;
	this->pPath = pPath;
	this->speed = speed;
//	lastSector = -1;
	lastFrame = -1;
	StateActivate();
}

int ITravelAction::CalcSpeedStep(CCircuitAI* circuit, float& stepSpeed)
{
	CCircuitUnit* unit = static_cast<CCircuitUnit*>(ownerList);
	const AIFloat3& pos = unit->GetPos(lastFrame);
	int pathMaxIndex = pPath->posPath.size() - 1;

//	int lastStep = pathIterator;
	float sqDistToStep = pos.SqDistance2D(pPath->posPath[pathIterator]);
	int step = std::min(pathIterator + increment, pathMaxIndex);
	float sqNextDistToStep = pos.SqDistance2D(pPath->posPath[step]);
	while ((sqNextDistToStep < sqDistToStep) && (pathIterator < pathMaxIndex)) {
		pathIterator = step;
		sqDistToStep = sqNextDistToStep;
		step = std::min(pathIterator + increment, pathMaxIndex);
		sqNextDistToStep = pos.SqDistance2D(pPath->posPath[step]);
	}

	if (/*(pathIterator == lastStep) && */((int)sqDistToStep > minSqDist)
		&& (pPath->path[pathIterator + pPath->start] == lastSector))
	{
		return -1;
	} else {
		stepSpeed = speed;
	}
	lastSector = pPath->path[pathIterator + pPath->start];

	if ((int)sqDistToStep <= minSqDist) {
		pathIterator = step;
		if (pathIterator == pathMaxIndex) {
			StateFinish();
			return circuit->GetCallback()->Unit_HasCommands(unit->GetId()) ? -2 : -1;
		}
	}

	return pathMaxIndex;
}

#ifdef DEBUG_VIS
void ITravelAction::Log(CCircuitAI* circuit)
{
	circuit->LOG("travel: %p | state: %i", this, state);
}
#endif

} // namespace circuit
