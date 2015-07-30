/*
 * SetupData.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_SETUPDATA_H_
#define SRC_CIRCUIT_STATIC_SETUPDATA_H_

#include "unit/AllyTeam.h"

#include "Game/GameSetup.h"

#include <vector>

namespace circuit {

class CSetupData {
public:
	CSetupData();
	virtual ~CSetupData();
	void Init(const std::vector<CAllyTeam*>& ats, CGameSetup::StartPosType spt = CGameSetup::StartPosType::StartPos_ChooseInGame);

	bool IsInitialized() const { return initialized; }
	bool CanChooseStartPos() const { return startPosType == CGameSetup::StartPos_ChooseInGame; }

	CAllyTeam* GetAllyTeam(int allyTeamId) const { return allyTeams[allyTeamId]; }
	const CAllyTeam::SBox& operator[](int idx) const { return allyTeams[idx]->GetStartBox(); }

private:
	bool initialized;
	CGameSetup::StartPosType startPosType;
	std::vector<CAllyTeam*> allyTeams;  // owner
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPDATA_H_
