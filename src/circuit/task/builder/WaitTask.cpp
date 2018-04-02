/*
 * WaitTask.cpp
 *
 *  Created on: May 29, 2017
 *      Author: rlcevg
 */

#include "task/builder/WaitTask.h"
#include "task/RetreatTask.h"
#include "task/TaskManager.h"
#include "module/BuilderManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBWaitTask::CBWaitTask(ITaskManager* mgr, int timeout)
		: IWaitTask(mgr, false, timeout)
{
}

CBWaitTask::~CBWaitTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBWaitTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	CCircuitDef* cdef = unit->GetCircuitDef();
	Unit* u = unit->GetUnit();
	const float healthPerc = u->GetHealth() / u->GetMaxHealth() - u->GetCaptureProgress() * 2.f;
	if ((healthPerc > cdef->GetRetreat()) && !unit->IsDisarmed(frame)) {
		return;
	}

	CRetreatTask* task = manager->GetCircuit()->GetBuilderManager()->EnqueueRetreat();
	manager->AssignTask(unit, task);
}

} // namespace circuit
