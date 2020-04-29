/*
 * Action.cpp
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 *      Origin: Randy Gaul (http://gamedevelopment.tutsplus.com/tutorials/the-action-list-data-structure-good-for-ui-ai-animations-and-more--gamedev-9264)
 */

#include "util/Action.h"

namespace circuit {

IAction::IAction(CActionList* owner)
		: ownerList(owner)
		, isBlocking(false)
		, state(State::ACTIVE)
{
}

IAction::~IAction()
{
}

void IAction::OnStart()
{
}

void IAction::OnEnd()
{
	state = State::HALT;
}

} // namespace circuit
