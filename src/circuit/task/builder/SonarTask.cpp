/*
 * SonarTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/SonarTask.h"
#include "unit/CircuitDef.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBSonarTask::CBSonarTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 float cost, float shake, int timeout)
		: ISensorTask(mgr, priority, [](CCircuitDef* cdef) { return cdef->IsSonar(); },
				buildDef, position, BuildType::SONAR, cost, shake, timeout)
{
}

CBSonarTask::~CBSonarTask()
{
}

} // namespace circuit
