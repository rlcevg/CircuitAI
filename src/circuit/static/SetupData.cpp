/*
 * SetupData.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "static/SetupData.h"
#include "util/utils.h"

#include "AIFloat3.h"

namespace circuit {

using namespace springai;

CSetupData::CSetupData() :
		initialized(false),
		startPosType(CGameSetup::StartPosType::StartPos_Fixed)
{
}

CSetupData::~CSetupData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	utils::free_clear(allyTeams);
}

void CSetupData::Init(const std::vector<CAllyTeam*>& ats, CGameSetup::StartPosType spt)
{
	allyTeams = ats;
	startPosType = spt;

	initialized = true;
}

bool CSetupData::IsInitialized()
{
	return initialized;
}

bool CSetupData::CanChooseStartPos()
{
	return startPosType == CGameSetup::StartPos_ChooseInGame;
}

CAllyTeam* CSetupData::GetAllyTeam(int allyTeamId)
{
	return allyTeams[allyTeamId];
}

const CAllyTeam::SBox& CSetupData::operator[](int idx) const
{
	return allyTeams[idx]->GetStartBox();
}

} // namespace circuit
