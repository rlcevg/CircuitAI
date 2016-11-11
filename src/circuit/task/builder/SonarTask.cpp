/*
 * SonarTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/SonarTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBSonarTask::CBSonarTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 float cost, float shake, int timeout)
		: ISensorTask(mgr, priority, buildDef, position, BuildType::SONAR, cost, shake, timeout)
{
}

CBSonarTask::~CBSonarTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
