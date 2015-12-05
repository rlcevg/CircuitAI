/*
 * GameAttribute.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#include "util/GameAttribute.h"
#include "util/utils.h"
#include "CircuitAI.h"

namespace circuit {

using namespace springai;

CGameAttribute::CGameAttribute()
		: isGameEnd(false)
{
	srand(time(nullptr));
}

CGameAttribute::~CGameAttribute()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CGameAttribute::SetGameEnd(bool value)
{
	if (isGameEnd == value) {
		return;
	}

	if (value) {
		for (CCircuitAI* circuit : circuits) {
			circuit->NotifyGameEnd();
		}
	}
	isGameEnd = value;
}

} // namespace circuit
