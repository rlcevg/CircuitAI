/*
 * RearmTask.cpp
 *
 *  Created on: Jan 26, 2016
 *      Author: rlcevg
 */

#include "task/RearmTask.h"

namespace circuit {

CRearmTask::CRearmTask(IUnitModule* mgr)
		: IUnitTask(mgr, Priority::NORMAL, Type::RETREAT, -1)
{
	// TODO Auto-generated constructor stub

}

CRearmTask::~CRearmTask()
{
	// TODO Auto-generated destructor stub
}

} // namespace circuit
