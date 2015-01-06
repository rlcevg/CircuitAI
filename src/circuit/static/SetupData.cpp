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

bool CSetupData::Box::ContainsPoint(const springai::AIFloat3& point) const
{
	return (point.x >= left) && (point.x <= right) &&
		   (point.z >= top) && (point.z <= bottom);
}

CSetupData::CSetupData() :
		initialized(false),
		startPosType(CGameSetup::StartPosType::StartPos_Fixed)
{
}

CSetupData::~CSetupData()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

void CSetupData::Init(std::vector<Box>& sb, CGameSetup::StartPosType spt)
{
	startBoxes = sb;
	startPosType = spt;
	initialized = true;
}

bool CSetupData::IsInitialized()
{
	return initialized;
}

bool CSetupData::IsEmpty()
{
	return startBoxes.empty();
}

bool CSetupData::CanChooseStartPos()
{
	return startPosType == CGameSetup::StartPos_ChooseInGame;
}

//int CSetupData::GetAllyTeamsCount()
//{
//	return startBoxes.size();
//}

const CSetupData::Box& CSetupData::operator[](int idx) const
{
	return startBoxes[idx];
}

} // namespace circuit
