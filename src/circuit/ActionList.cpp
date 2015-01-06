/*
 * ActionList.cpp
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 *      Origin: Randy Gaul (http://gamedevelopment.tutsplus.com/tutorials/the-action-list-data-structure-good-for-ui-ai-animations-and-more--gamedev-9264)
 */

#include "ActionList.h"
#include "Action.h"

namespace circuit {

CActionList::CActionList()
{
	// TODO Auto-generated constructor stub

}

CActionList::~CActionList()
{
	// TODO Auto-generated destructor stub
}

void CActionList::Update(float dt)
{
	unsigned int lanes = 0;
	for (auto action : actions) {
		if(lanes & action->lanes) {
			continue;
		}

	    action->Update(dt);
		if (action->isBlocking) {
			lanes |= action->lanes;
//			break;
		}

		if (action->isFinished) {
			action->OnEnd();
			action = this->Remove(action);
		}
	}
}

} // namespace circuit
