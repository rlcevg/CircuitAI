/*
 * GameTask.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "util/GameTask.h"
#include "util/Utils.h"

namespace circuit {

std::shared_ptr<CGameTask> CGameTask::emptyTask = std::make_shared<CGameTask>([]() { return; });

CGameTask::~CGameTask()
{
}

void CGameTask::Run()
{
	CGameTask::_Impl_base* __t = static_cast<CGameTask::_Impl_base*>(__b.get());
	CGameTask::__shared_base_type __local;
	__local.swap(__t->_M_this_ptr);

	__t->_M_run();
}

} // namespace circuit
