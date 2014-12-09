/*
 * GameAttribute.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#include "GameAttribute.h"
#include "CircuitAI.h"
#include "utils.h"

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

bool CGameAttribute::IsGameEnd()
{
	return gameEnd;
}

void CGameAttribute::RegisterAI(CCircuitAI* circuit)
{
	circuits.insert(circuit);
}

void CGameAttribute::UnregisterAI(CCircuitAI* circuit)
{
	circuits.erase(circuit);
}

CSetupData& CGameAttribute::GetSetupData()
{
	return setupData;
}

CMetalData& CGameAttribute::GetMetalData()
{
	return metalData;
}

} // namespace circuit
