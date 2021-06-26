/*
 * ConvertTask.cpp
 *
 *  Created on: Jun 21, 2021
 *      Author: rlcevg
 */

#include "task/builder/ConvertTask.h"
#include "task/TaskManager.h"
#include "module/EconomyManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBConvertTask::CBConvertTask(ITaskManager* mgr, Priority priority,
							 CCircuitDef* buildDef, const AIFloat3& position,
							 float cost, float shake, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::CONVERT, cost, shake, timeout)
{
}

CBConvertTask::~CBConvertTask()
{
}

void CBConvertTask::Update()
{
	IBuilderTask::Update();
	if (isDead || (target != nullptr)) {
		return;
	}

	CEconomyManager* economyMgr = manager->GetCircuit()->GetEconomyManager();
	if (economyMgr->GetEnergyCur() < economyMgr->GetEnergyStore() * 0.55f) {
		manager->AbortTask(this);
	}
}

} // namespace circuit
