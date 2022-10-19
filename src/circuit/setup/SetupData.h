/*
 * SetupData.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_SETUPDATA_H_
#define SRC_CIRCUIT_STATIC_SETUPDATA_H_

#include "util/math/Region.h"

#include "Game/GameSetup.h"

#include <map>
#include <vector>

namespace springai {
	class Game;
}

namespace circuit {

class CCircuitAI;
class CAllyTeam;
class CMap;

class CSetupData {
public:
	using BoxMap = std::map<int, utils::CRegion>;  // <start_box_id, box>
	using AllyMap = std::vector<CAllyTeam*>;

	CSetupData();
	~CSetupData();
	void ParseSetupScript(CCircuitAI* circuit, const char* setupScript);

	bool IsInitialized() const { return isInitialized; }
	bool CanChooseStartPos() const { return false/*startPosType == CGameSetup::StartPos_ChooseInGame*/; }

	CAllyTeam* GetAllyTeam(int allyTeamId) { return allyTeams[allyTeamId]; }
	const utils::CRegion& GetStartBox(int boxId) { return boxes[boxId]; }

private:
	void Init(AllyMap&& ats, BoxMap&& bm,
			  CGameSetup::StartPosType spt = CGameSetup::StartPosType::StartPos_ChooseInGame);
	BoxMap ReadStartBoxes(const std::string& script, CMap* map, springai::Game* game);

	bool isInitialized;
	CGameSetup::StartPosType startPosType;
	AllyMap allyTeams;  // owner
	BoxMap boxes;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPDATA_H_
