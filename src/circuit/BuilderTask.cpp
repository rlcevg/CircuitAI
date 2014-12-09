/*
 * BuilderTask.cpp
 *
 *  Created on: Sep 11, 2014
 *      Author: rlcevg
 */

#include "BuilderTask.h"
#include "CircuitUnit.h"
#include "utils.h"

#include "UnitDef.h"

#include <algorithm>

namespace circuit {

using namespace springai;

CBuilderTask::CBuilderTask(Priority priority,
		UnitDef* buildDef, const AIFloat3& position,
		TaskType type, float cost, int timeout) :
				IConstructTask(priority, buildDef, position, ConstructType::BUILDER),
				type(type),
				cost(cost),
				timeout(timeout),
				target(nullptr),
				buildPos(-RgtVector),
				buildPower(.0f)
{

}

CBuilderTask::~CBuilderTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CBuilderTask::CanAssignTo(CCircuitUnit* unit)
{
	std::vector<UnitDef*> opts = unit->GetDef()->GetBuildOptions();
//	bool valid = (std::find(opts.begin(), opts.end(), buildDef) != opts.end()) && (cost > buildPower * MIN_BUILD_TIME);
	bool valid = false;
	for (auto opt : opts) {
		if (opt->GetUnitDefId() == buildDef->GetUnitDefId()) {
			valid = true;
			break;
		}
	}
	utils::free_clear(opts);
	return valid && (cost > buildPower * MIN_BUILD_SEC);
}

void CBuilderTask::AssignTo(CCircuitUnit* unit)
{
	IUnitTask::AssignTo(unit);
	buildPower += unit->GetDef()->GetBuildSpeed();
}

void CBuilderTask::RemoveAssignee(CCircuitUnit* unit)
{
	IUnitTask::RemoveAssignee(unit);
	buildPower -= unit->GetDef()->GetBuildSpeed();
}

CBuilderTask::TaskType CBuilderTask::GetType()
{
	return type;
}

float CBuilderTask::GetBuildPower()
{
	return buildPower;
}

float CBuilderTask::GetCost()
{
	return cost;
}

int CBuilderTask::GetTimeout()
{
	return timeout;
}

void CBuilderTask::SetBuildPos(const AIFloat3& pos)
{
	buildPos = pos;
}

const AIFloat3& CBuilderTask::GetBuildPos() const
{
	return buildPos;
}

void CBuilderTask::SetTarget(CCircuitUnit* unit)
{
	target = unit;
}

CCircuitUnit* CBuilderTask::GetTarget()
{
	return target;
}

} // namespace circuit
