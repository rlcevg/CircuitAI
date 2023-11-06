/*
 * WaitTask.cpp
 *
 *  Created on: May 29, 2017
 *      Author: rlcevg
 */

#include "task/builder/WaitTask.h"
#include "task/RetreatTask.h"
#include "module/BuilderManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

namespace circuit {

using namespace springai;

CBWaitTask::CBWaitTask(IUnitModule* mgr, int timeout)
		: IWaitTask(mgr, false, timeout)
{
}

CBWaitTask::~CBWaitTask()
{
}

void CBWaitTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const float healthPerc = unit->GetHealthPercent();
	if ((healthPerc > cdef->GetRetreat()) && !unit->IsDisarmed(frame)) {
		return;
	}

	CRetreatTask* task = manager->GetCircuit()->GetBuilderManager()->EnqueueRetreat();
	manager->AssignTask(unit, task);
}

} // namespace circuit
