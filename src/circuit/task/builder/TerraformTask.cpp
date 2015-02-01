/*
 * TerraformTask.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: rlcevg
 */

#include "task/builder/TerraformTask.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CBTerraformTask::CBTerraformTask(ITaskManager* mgr, Priority priority,
								 UnitDef* buildDef, const AIFloat3& position,
								 float cost, int timeout) :
		IBuilderTask(mgr, priority, buildDef, position, BuildType::TERRAFORM, cost, timeout)
{
}

CBTerraformTask::~CBTerraformTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CBTerraformTask::Execute(CCircuitUnit* unit)
{
	// TODO: Terraform
}

} // namespace circuit
