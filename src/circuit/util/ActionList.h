/*
 * ActionList.h
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 *      Origin: Randy Gaul (http://gamedevelopment.tutsplus.com/tutorials/the-action-list-data-structure-good-for-ui-ai-animations-and-more--gamedev-9264)
 */

#ifndef SRC_CIRCUIT_ACTIONLIST_H_
#define SRC_CIRCUIT_ACTIONLIST_H_

#include <list>

namespace circuit {

class IAction;

class CActionList {
public:
	CActionList();
	virtual ~CActionList();

	void Update();

	void PushFront(IAction* action);
	void PushBack(IAction* action);
	void InsertBefore(IAction* action);
	void InsertBefore(std::list<IAction*>::iterator it, IAction* action);
	void InsertAfter(IAction* action);
	void InsertAfter(std::list<IAction*>::iterator it, IAction* action);
	IAction* Remove(IAction* action);
	std::list<IAction*>::iterator Remove(std::list<IAction*>::iterator it);

	IAction* Begin();
	IAction* End();

	bool IsEmpty() const;

	std::list<IAction*>::iterator itAction;

protected:
	int startFrame;
	int duration;
	std::list<IAction*> actions;  // owner
};

} // namespace circuit

#endif // SRC_CIRCUIT_ACTIONLIST_H_