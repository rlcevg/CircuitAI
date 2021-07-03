/*
 * GameAttribute.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#include "util/GameAttribute.h"
#include "util/Utils.h"
#include "CircuitAI.h"

namespace circuit {

using namespace springai;

CGameAttribute::CGameAttribute()
		: isInitialized(false)
		, isGameEnd(false)
{
}

CGameAttribute::~CGameAttribute()
{
}

void CGameAttribute::Init(unsigned int seed)
{
	srand(seed);
	isInitialized = true;
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
