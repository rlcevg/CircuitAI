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
class CCircuitAI;

class IAction {
protected:
	IAction(CActionList* owner);
public:
	virtual ~IAction();

	virtual void Update(CCircuitAI* circuit) = 0;
	virtual void OnStart(void);
	virtual void OnEnd(void);
	bool isFinished;
	bool isBlocking;
	int startFrame;
	int duration;

private:
	CActionList* ownerList;
};

} // namespace circuit

#endif // SRC_CIRCUIT_ACTION_H_
