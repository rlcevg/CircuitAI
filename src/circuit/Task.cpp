/*
 * Task.cpp
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#include "Task.h"

#include <functional>

namespace circuit {

CTask::~CTask()
{
	printf("<DEBUG> Entering:  %s\n", __PRETTY_FUNCTION__);
}

void CTask::Run()
{
	CTask::_Impl_base* __t = static_cast<CTask::_Impl_base*>(__b.get());
	CTask::__shared_base_type __local;
	__local.swap(__t->_M_this_ptr);

	__t->_M_run();
}

} // namespace circuit
