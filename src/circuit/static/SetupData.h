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
	using BoxMap = std::map<int, CAllyTeam::SBox>;

	CSetupData();
	virtual ~CSetupData();
	void Init(const std::vector<CAllyTeam*>& ats, const BoxMap& bm,
			  CGameSetup::StartPosType spt = CGameSetup::StartPosType::StartPos_ChooseInGame);

	bool IsInitialized() const { return initialized; }
	bool CanChooseStartPos() const { return startPosType == CGameSetup::StartPos_ChooseInGame; }

	CAllyTeam* GetAllyTeam(int allyTeamId) const { return allyTeams[allyTeamId]; }
	const CAllyTeam::SBox& GetStartBox(int idx) { return boxes[idx]; }

private:
	bool initialized;
	CGameSetup::StartPosType startPosType;
	std::vector<CAllyTeam*> allyTeams;  // owner
	BoxMap boxes;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPDATA_H_
