/*
 * RadarTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/RadarTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBRadarTask::CBRadarTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::RADAR, cost, timeout)
{
}

CBRadarTask::~CBRadarTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
