/*
 * GameAttribute.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#include "static/GameAttribute.h"
#include "CircuitAI.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CGameAttribute::CGameAttribute() :
		gameEnd(false)
{
	srand(time(nullptr));
}

CGameAttribute::~CGameAttribute()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CGameAttribute::SetGameEnd(bool value)
{
	if (gameEnd != value) {
		if (value) {
			for (auto circuit : circuits) {
				circuit->NotifyGameEnd();
			}
		}
		gameEnd = value;
	}
}

} // namespace circuit
