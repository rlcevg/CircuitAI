/*
 * RetreatAction.cpp
 *
 *  Created on: Jan 27, 2015
 *      Author: rlcevg
 */

#include "task/action/RetreatAction.h"
#include "task/UnitTask.h"
#include "CircuitAI.h"
#include "module/FactoryManager.h"
#include "static/SetupManager.h"
#include "unit/CircuitUnit.h"
#include "util/utils.h"

#include "AIFloat3.h"

namespace circuit {

using namespace springai;

CRetreatAction::CRetreatAction(IUnitTask* owner) :
		ITaskAction(owner)
{
}

CRetreatAction::~CRetreatAction()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CRetreatAction::Update(CCircuitAI* circuit)
{
	IUnitTask* task = static_cast<IUnitTask*>(ownerList);
	for (auto ass : task->GetAssignees()) {
		CCircuitUnit* haven = circuit->GetFactoryManager()->GetClosestHaven(ass);
		const AIFloat3& pos = (haven != nullptr) ? haven->GetUnit()->GetPos() : circuit->GetSetupManager()->GetStartPos();
		// TODO: push MoveAction into unit?
		ass->GetUnit()->MoveTo(pos, UNIT_COMMAND_OPTION_INTERNAL_ORDER, FRAMES_PER_SEC * 1);
	}
}

void CRetreatAction::OnStart()
{

}

void CRetreatAction::OnEnd()
{

}

} // namespace circuit
