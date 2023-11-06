/*
 * SensorTask.cpp
 *
 *  Created on: Nov 9, 2016
 *      Author: rlcevg
 */

#include "task/common/SensorTask.h"
#include "module/FactoryManager.h"
#include "CircuitAI.h"

#include "spring/SpringCallback.h"

namespace circuit {

using namespace springai;

ISensorTask::ISensorTask(IUnitModule* mgr, Priority priority, std::function<bool (CCircuitDef*)> isSensor,
						 CCircuitDef* buildDef, const AIFloat3& position, BuildType buildType,
						 SResource cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, buildType, cost, shake, timeout)
		, isSensorTest(isSensor)
{
}

ISensorTask::ISensorTask(IUnitModule* mgr, std::function<bool (CCircuitDef*)> isSensor, BuildType buildType)
		: IBuilderTask(mgr, Type::BUILDER, buildType)
		, isSensorTest(isSensor)
{
}

ISensorTask::~ISensorTask()
{
}

bool ISensorTask::CanAssignTo(CCircuitUnit* unit) const
{
	if (manager->GetCircuit()->GetFactoryManager()->GetFactoryCount() == 0) {
		return false;
	}
	return IBuilderTask::CanAssignTo(unit);
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
		CCircuitDef::Id defId = clb->Unit_GetDefId(auId);
		if (isSensorTest(circuit->GetCircuitDef(defId))) {
			isBuilt = true;
			break;
		}
	}
	if (isBuilt) {
		manager->AbortTask(this);
	}
}

} // namespace circuit
