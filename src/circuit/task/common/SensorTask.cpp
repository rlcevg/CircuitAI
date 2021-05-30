/*
 * SensorTask.cpp
 *
 *  Created on: Nov 9, 2016
 *      Author: rlcevg
 */

#include "task/common/SensorTask.h"
#include "task/TaskManager.h"
#include "CircuitAI.h"

#include "spring/SpringCallback.h"

namespace circuit {

using namespace springai;

ISensorTask::ISensorTask(ITaskManager* mgr, Priority priority,
		 	 	 	 	 CCircuitDef* buildDef, const AIFloat3& position, BuildType buildType,
						 float cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, buildType, cost, shake, timeout)
{
}

ISensorTask::~ISensorTask()
{
}

void ISensorTask::Update()
{
	IBuilderTask::Update();
	if (isDead || (target != nullptr)) {
		return;
	}

	CCircuitAI* circuit = manager->GetCircuit();
	bool isBuilt = false;
	COOAICallback* clb = circuit->GetCallback();
	auto& friendlies = clb->GetFriendlyUnitIdsIn(GetPosition(), 500.f);
	for (int auId : friendlies) {
		if (auId == -1) {
			continue;
		}
		CCircuitDef::Id defId = clb->Unit_GetDefId(auId);
		if (defId == buildDef->GetId()) {
			isBuilt = true;
			break;
		}
	}
	if (isBuilt) {
		manager->AbortTask(this);
	}
}

} // namespace circuit
