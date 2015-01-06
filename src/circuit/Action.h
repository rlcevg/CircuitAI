/*
 * Action.h
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 *      Origin: Randy Gaul (http://gamedevelopment.tutsplus.com/tutorials/the-action-list-data-structure-good-for-ui-ai-animations-and-more--gamedev-9264)
 */

#ifndef SRC_CIRCUIT_ACTION_H_
#define SRC_CIRCUIT_ACTION_H_

namespace circuit {

class CActionList;

class IAction {
protected:
	IAction();
public:
	virtual ~IAction();

	virtual void Update(float dt) = 0;
	virtual void OnStart(void) = 0;
	virtual void OnEnd(void) = 0;
	bool isFinished;
	bool isBlocking;
	unsigned lanes;
	float elapsed;
	float duration;

private:
	CActionList* ownerList;
};

} // namespace circuit

#endif // SRC_CIRCUIT_ACTION_H_
