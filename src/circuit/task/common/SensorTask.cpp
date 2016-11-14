/*
 * SensorTask.cpp
 *
 *  Created on: Nov 9, 2016
 *      Author: rlcevg
 */

#include "task/common/SensorTask.h"
#include "task/TaskManager.h"
#include "CircuitAI.h"

#include "OOAICallback.h"

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
	auto friendlies = std::move(circuit->GetCallback()->GetFriendlyUnitsIn(GetPosition(), 500.f));
	for (Unit* au : friendlies) {
		if (au == nullptr) {
			continue;
		}
		UnitDef* udef = au->GetDef();
		CCircuitDef::Id defId = udef->GetUnitDefId();
		delete udef;
		if (defId == buildDef->GetId()) {
			isBuilt = true;
			break;
		}
	}
	utils::free_clear(friendlies);
	if (isBuilt) {
		manager->AbortTask(this);
	}
}

} // namespace circuit
