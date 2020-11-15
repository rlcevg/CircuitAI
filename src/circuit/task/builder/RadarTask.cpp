/*
 * RadarTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/RadarTask.h"
#include "module/FactoryManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBRadarTask::CBRadarTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, const AIFloat3& position,
						 float cost, float shake, int timeout)
		: ISensorTask(mgr, priority, buildDef, position, BuildType::RADAR, cost, shake, timeout)
{
}

CBRadarTask::~CBRadarTask()
{
}

bool CBRadarTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (manager->GetCircuit()->GetFactoryManager()->GetFactoryCount() == 0) {
		return false;
	}
	return IBuilderTask::CanAssignTo(unit);
}

} // namespace circuit
