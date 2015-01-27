/*
 * ActionList.cpp
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 *      Origin: Randy Gaul (http://gamedevelopment.tutsplus.com/tutorials/the-action-list-data-structure-good-for-ui-ai-animations-and-more--gamedev-9264)
 */

#include "util/ActionList.h"
#include "util/Action.h"
#include "util/utils.h"

#include <algorithm>

namespace circuit {

CActionList::CActionList() :
		startFrame(-1),
		duration(-1)
{
}

CActionList::~CActionList()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(actions);
}

void CActionList::Update(CCircuitAI* circuit)
{
	itAction = actions.begin();
	while (itAction != actions.end()) {
		IAction* action = *itAction;
	    action->Update(circuit);

		if (action->isBlocking) {
			break;
		}

		if (action->isFinished) {
			action->OnEnd();
			itAction = this->Remove(itAction);
		} else {
			++itAction;
		}
	}
}

void CActionList::PushFront(IAction* action)
{
	actions.push_front(action);
}

void CActionList::PushBack(IAction* action)
{
	actions.push_back(action);
}

void CActionList::InsertBefore(IAction* action)
{
	auto it = std::find(actions.begin(), actions.end(), action);
	actions.insert(it, action);
}

void CActionList::InsertBefore(decltype(actions)::iterator it, IAction* action)
{
	actions.insert(it, action);
}

void CActionList::InsertAfter(IAction* action)
{
	auto it = std::find(actions.begin(), actions.end(), action);
	actions.insert(++it, action);
}

void CActionList::InsertAfter(decltype(actions)::iterator it, IAction* action)
{
	auto itIns = it;
	actions.insert(++itIns, action);
}

IAction* CActionList::Remove(IAction* action)
{
	auto it = std::find(actions.begin(), actions.end(), action);
	return *Remove(it);
}

decltype(CActionList::actions)::iterator CActionList::Remove(decltype(actions)::iterator it)
{
	return actions.erase(it);
}

IAction* CActionList::Begin()
{
	return actions.front();
}

IAction* CActionList::End()
{
	return actions.back();
}

bool CActionList::IsEmpty() const
{
	return actions.empty();
}

} // namespace circuit
