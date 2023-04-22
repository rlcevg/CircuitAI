/*
 * RadarTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/RadarTask.h"
#include "unit/CircuitDef.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBRadarTask::CBRadarTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 SResource cost, float shake, int timeout)
		: ISensorTask(mgr, priority, [](CCircuitDef* cdef) { return cdef->IsRadar(); },
				buildDef, position, BuildType::RADAR, cost, shake, timeout)
{
}

CBRadarTask::CBRadarTask(ITaskManager* mgr)
		: ISensorTask(mgr, [](CCircuitDef* cdef) { return cdef->IsRadar(); }, BuildType::RADAR)
{
}

CBRadarTask::~CBRadarTask()
{
}

} // namespace circuit
