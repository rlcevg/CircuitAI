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

CActionList::CActionList()
		: startFrame(-1)
		, blocker(nullptr)
{
}

CActionList::~CActionList()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	Clear();
}

void CActionList::Update(CCircuitAI* circuit)
{
	std::list<IAction*>::iterator itAction = actions.begin();
	while (itAction != actions.end()) {
		IAction* action = *itAction;
		if (action->IsActive()) {
			action->Update(circuit);
		}

		if (action->IsBlocking()) {
			blocker = action;
			return;
		}

		if (action->IsFinished()) {
			action->OnEnd();
			itAction = Remove(itAction);
		} else {
			++itAction;
		}
	}
	blocker = nullptr;
}

void CActionList::PushFront(IAction* action)
{
	actions.push_front(action);
	action->OnStart();
}

void CActionList::PushBack(IAction* action)
{
	actions.push_back(action);
	action->OnStart();
}

void CActionList::InsertBefore(IAction* action)
{
	auto it = std::find(actions.begin(), actions.end(), action);
	InsertBefore(it, action);
}

void CActionList::InsertBefore(std::list<IAction*>::iterator it, IAction* action)
{
	actions.insert(it, action);
	action->OnStart();
}

void CActionList::InsertAfter(IAction* action)
{
	auto it = std::find(actions.begin(), actions.end(), action);
	actions.insert(++it, action);
	action->OnStart();
}

void CActionList::InsertAfter(decltype(actions)::iterator it, IAction* action)
{
	auto itIns = it;
	actions.insert(++itIns, action);
	action->OnStart();
}

void CActionList::Clear()
{
	utils::free_clear(actions);
}

std::list<IAction*>::iterator CActionList::Remove(std::list<IAction*>::iterator it)
{
	delete *it;
	return actions.erase(it);
}

} // namespace circuit
