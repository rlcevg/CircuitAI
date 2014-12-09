/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef GAMEATTRIBUTE_H_
#define GAMEATTRIBUTE_H_

#include "SetupData.h"
#include "MetalData.h"

#include <unordered_set>

namespace springai {
	class Pathing;
}

namespace circuit {

class CCircuitAI;

class CGameAttribute {
public:
	CGameAttribute();
	virtual ~CGameAttribute();

	void SetGameEnd(bool value);
	bool IsGameEnd();
	void RegisterAI(CCircuitAI* circuit);
	void UnregisterAI(CCircuitAI* circuit);

	CSetupData& GetSetupData();
	CMetalData& GetMetalData();

private:
	bool gameEnd;
	std::unordered_set<CCircuitAI*> circuits;
	CSetupData setupData;
	CMetalData metalData;
};

} // namespace circuit

#endif // GAMEATTRIBUTE_H_
