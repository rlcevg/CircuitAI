/*
 * SetupManager.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "SetupManager.h"
#include "utils.h"

#include "AIFloat3.h"

namespace circuit {

using namespace springai;

bool CSetupManager::Box::ContainsPoint(const springai::AIFloat3& point) const
{
	return (point.x >= left) && (point.x <= right) &&
		   (point.z >= top) && (point.z <= bottom);
}

CSetupManager::CSetupManager(std::vector<Box>& startBoxes, CGameSetup::StartPosType startPosType) :
		startBoxes(startBoxes),
		startPosType(startPosType)
{
}

CSetupManager::~CSetupManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CSetupManager::IsEmpty()
{
	return startBoxes.empty();
}

bool CSetupManager::CanChooseStartPos()
{
	return startPosType == CGameSetup::StartPos_ChooseInGame;
}

const CSetupManager::Box& CSetupManager::operator[](int idx) const
{
	return startBoxes[idx];
}

} // namespace circuit
