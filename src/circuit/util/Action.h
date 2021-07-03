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
public:
	enum class State: char {NONE = 0, WAIT, ACTIVE, FINISH, HALT, _SIZE_};

protected:
	IAction(CActionList* owner);
public:
	virtual ~IAction();

	virtual void Update(CCircuitAI* circuit) = 0;
	virtual void OnStart();
	virtual void OnEnd();

	void SetBlocking(bool value) { isBlocking = value; }
	bool IsBlocking() const { return isBlocking; }

	void StateWait() { state = State::WAIT; }
	bool IsWait() const { return state == State::WAIT; }

	void StateActivate() { state = State::ACTIVE; }
	bool IsActive() const { return state == State::ACTIVE; }

	void StateFinish() { state = State::FINISH; }
	bool IsFinishing() const { return state == State::FINISH; }

	void StateHalt() { state = State::HALT; }
	bool IsFinished() const { return state >= State::FINISH; }

	void SetState(State value) { state = value; }
	State GetState() const { return state; }

protected:
	CActionList* ownerList;

	bool isBlocking;
	State state;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_ACTION_H_
