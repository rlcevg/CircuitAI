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

IConstructTask::IConstructTask(Priority priority, Type type,
		UnitDef* buildDef, const AIFloat3& position) :
				IUnitTask(priority, type),
				buildDef(buildDef),
				position(position)
{
}

IConstructTask::~IConstructTask()
{
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
