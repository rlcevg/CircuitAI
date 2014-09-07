/*
 * GameTask.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "GameTask.h"

namespace circuit {

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

void CGameTask::SetTerminate(bool value)
{
	terminate = value;
}

bool CGameTask::GetTerminate()
{
	return terminate;
}

} // namespace circuit
