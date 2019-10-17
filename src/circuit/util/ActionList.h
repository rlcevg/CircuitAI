/*
 * ActionList.h
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 *      Origin: Randy Gaul (http://gamedevelopment.tutsplus.com/tutorials/the-action-list-data-structure-good-for-ui-ai-animations-and-more--gamedev-9264)
 */

#ifndef SRC_CIRCUIT_UTIL_ACTIONLIST_H_
#define SRC_CIRCUIT_UTIL_ACTIONLIST_H_

#include "unit/action/IdleAction.h"

#include <deque>

namespace circuit {

class IAction;
class CCircuitAI;

class CActionList {
public:
	CActionList();
	virtual ~CActionList();

	void Update(CCircuitAI* circuit);

	void PushFront(IAction* action);
	void PushBack(IAction* action);
	void InsertBefore(IAction* action);
	void InsertBefore(std::deque<IAction*>::iterator it, IAction* action);
	void InsertAfter(IAction* action);
	void InsertAfter(std::deque<IAction*>::iterator it, IAction* action);

	IAction* Blocker() const { return blocker; }

	bool IsEmpty() const { return actions.empty(); }
	void Clear();

protected:
	IAction* Begin() const { return IsEmpty() ? &idleAction : actions.front(); }
	IAction* End() const { return IsEmpty() ? &idleAction : actions.back(); }
	std::deque<IAction*>::iterator Remove(std::deque<IAction*>::iterator it);

	int startFrame;
	std::deque<IAction*> actions;  // owner
	IAction* blocker;

	static CIdleAction idleAction;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_ACTIONLIST_H_
