/*
 * StuckTask.cpp
 *
 *  Created on: Sep 12, 2015
 *      Author: rlcevg
 */

#include "task/StuckTask.h"
#include "task/TaskManager.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CStuckTask::CStuckTask(ITaskManager* mgr)
		: IUnitTask(mgr, Priority::NORMAL, Type::STUCK)
{
}

CStuckTask::~CStuckTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CStuckTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);

	delete this;
}

void CStuckTask::Execute(CCircuitUnit* unit)
{
	int frame = manager->GetCircuit()->GetLastFrame();
	AIFloat3 pos = unit->GetPos(frame);
	AIFloat3 d((float)rand() / RAND_MAX - 0.5f, 0.0f, (float)rand() / RAND_MAX - 0.5f);
	d.Normalize();
	pos += d * SQUARE_SIZE * 20;
	unit->GetUnit()->MoveTo(pos, 0, frame + FRAMES_PER_SEC);
}

void CStuckTask::Update()
{
}

void CStuckTask::OnUnitIdle(CCircuitUnit* unit)
{
	RemoveAssignee(unit);
}

void CStuckTask::OnUnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker)
{
}

void CStuckTask::OnUnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker)
{
	RemoveAssignee(unit);
}

} // namespace circuit
