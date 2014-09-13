/*
 * ConstructTask.cpp
 *
 *  Created on: Sep 12, 2014
 *      Author: rlcevg
 */

#include "ConstructTask.h"
#include "utils.h"

namespace circuit {

using namespace springai;

IConstructTask::IConstructTask(Priority priority, int quantity, AIFloat3& position, std::list<IConstructTask*>& owner) :
		IUnitTask(priority),
		quantity(quantity),
		position(position),
		owner(&owner)
{
	owner.push_front(this);
}

IConstructTask::~IConstructTask()
{
}

void IConstructTask::Progress()
{
	quantity--;
}

void IConstructTask::Regress()
{
	quantity++;
}

bool IConstructTask::IsDone()
{
	return quantity <= 0;
}

void IConstructTask::MarkCompleted()
{
	IUnitTask::MarkCompleted();
	owner->remove(this);
}

} // namespace circuit
