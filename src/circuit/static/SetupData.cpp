/*
 * SetupData.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "static/SetupData.h"
#include "util/utils.h"

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

} // namespace circuit
