/*
 * ConstructTask.cpp
 *
 *  Created on: Sep 12, 2014
 *      Author: rlcevg
 */

#include "ConstructTask.h"
#include "utils.h"

namespace circuit {

using namespace springai;

IConstructTask::IConstructTask(Priority priority,
		AIFloat3& position, std::list<IConstructTask*>& owner, ConstructType conType) :
				IUnitTask(priority),
				position(position),
				owner(&owner),
				conType(conType)
{
	owner.push_front(this);
}

IConstructTask::~IConstructTask()
{
}

void IConstructTask::MarkCompleted()
{
	IUnitTask::MarkCompleted();
	owner->remove(this);
}

IConstructTask::ConstructType IConstructTask::GetConstructType()
{
	return conType;
}

AIFloat3& IConstructTask::GetPos()
{
	return position;
}

} // namespace circuit
