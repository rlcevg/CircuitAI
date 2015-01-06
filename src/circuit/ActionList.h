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

	void Update(float dt);

	void PushFront(IAction* action);
	void PushBack(IAction* action);
	void InsertBefore(IAction* action);
	void InsertAfter(IAction* action);
	IAction* Remove(IAction* action);

	IAction* Begin(void);
	IAction* End(void);

	bool IsEmpty(void) const;
	float TimeLeft(void) const;
	bool IsBlocking(void) const;

private:
	float duration;
	float timeElapsed;
	float percentDone;
	bool blocking;
	unsigned lanes;
	// FIXME: owner?
	std::list<IAction*> actions; // can be a vector or linked list
};

} // namespace circuit

#endif // SRC_CIRCUIT_ACTIONLIST_H_
