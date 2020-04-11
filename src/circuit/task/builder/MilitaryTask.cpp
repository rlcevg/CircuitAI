/*
 * MilitaryTask.cpp
 *
 *  Created on: Apr 10, 2020
 *      Author: rlcevg
 */

#include "task/builder/MilitaryTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

CBMilitaryTask::CBMilitaryTask(ITaskManager* mgr)
		: IBuilderTask(mgr, Priority::NOW, nullptr, -RgtVector,
					   Type::FIGHTER, BuildType::DEFAULT, 0.f)
{
}

CBMilitaryTask::~CBMilitaryTask()
{
}

void CBMilitaryTask::RemoveAssignee(CCircuitUnit* unit)
{
	IBuilderTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CBMilitaryTask::Start(CCircuitUnit* unit)
{
	CCircuitAI* circuit = manager->GetCircuit();
	CBuilderManager* builderManager = circuit->GetBuilderManager();
	CMilitaryManager* militaryManager = circuit->GetMilitaryManager();
	IUnitTask* task = militaryManager->EnqueueDefendComm();
	decltype(units) tmpUnits = units;
	for (CCircuitUnit* unit : tmpUnits) {
		builderManager->UnitDestroyed(unit, nullptr);
		militaryManager->AssignTask(unit, task);
	}
}

} // namespace circuit
