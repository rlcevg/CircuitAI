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
		AIFloat3& position, ConstructType conType) :
				IUnitTask(priority),
				position(position),
				conType(conType)
{
}

IConstructTask::~IConstructTask()
{
}

IConstructTask::ConstructType IConstructTask::GetConstructType()
{
	return conType;
}

const AIFloat3& IConstructTask::GetPos() const
{
	return position;
}

} // namespace circuit
