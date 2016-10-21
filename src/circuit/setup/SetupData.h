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
	using BoxMap = std::map<int, CAllyTeam::SBox>;  // <start_box_id, box>
	using AllyMap = std::vector<CAllyTeam*>;

	CSetupData();
	virtual ~CSetupData();
	void ParseSetupScript(CCircuitAI* circuit, const char* setupScript);
	void Init(const AllyMap& ats, const BoxMap& bm,
			  CGameSetup::StartPosType spt = CGameSetup::StartPosType::StartPos_ChooseInGame);

	bool IsInitialized() const { return isInitialized; }
	bool CanChooseStartPos() const { return startPosType == CGameSetup::StartPos_ChooseInGame; }

	CAllyTeam* GetAllyTeam(int allyTeamId) const { return allyTeams[allyTeamId]; }
	const CAllyTeam::SBox& GetStartBox(int boxId) { return boxes[boxId]; }

private:
	bool isInitialized;
	CGameSetup::StartPosType startPosType;
	AllyMap allyTeams;  // owner
	BoxMap boxes;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPDATA_H_
