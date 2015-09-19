/*
 * Action.h
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 *      Origin: Randy Gaul (http://gamedevelopment.tutsplus.com/tutorials/the-action-list-data-structure-good-for-ui-ai-animations-and-more--gamedev-9264)
 */

#ifndef SRC_CIRCUIT_UTIL_ACTION_H_
#define SRC_CIRCUIT_UTIL_ACTION_H_

namespace circuit {

class CActionList;
class CCircuitAI;

class IAction {
protected:
	IAction(CActionList* owner);
public:
	virtual ~IAction();

	virtual void Update(CCircuitAI* circuit) = 0;
	virtual void OnStart();
	virtual void OnEnd();

	void SetFinished(bool val) { isFinished = val; }
	bool IsFinished() const { return isFinished; }
	void SetBlocking(bool val) { isBlocking = val; }
	bool IsBlocking() const { return isBlocking; }
	void SetActive(bool val) { isActive = val; }
	bool IsActive() const { return isActive; }

protected:
	CActionList* ownerList;

	bool isFinished;
	bool isBlocking;
	bool isActive;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_ACTION_H_
