/*
 * ConstructTask.cpp
 *
 *  Created on: Sep 12, 2014
 *      Author: rlcevg
 */

#include "task/ConstructTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

IConstructTask::IConstructTask(Priority priority,
		UnitDef* buildDef, const AIFloat3& position, ConstructType conType) :
				IUnitTask(priority),
				buildDef(buildDef),
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

UnitDef* IConstructTask::GetBuildDef()
{
	return buildDef;
}

} // namespace circuit
