/*
 * SetupManager.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "SetupManager.h"

namespace circuit {

CSetupManager::CSetupManager(std::vector<Box>& startBoxes, CGameSetup::StartPosType startPosType) :
		startBoxes(startBoxes),
		startPosType(startPosType)
{
}

CSetupManager::~CSetupManager()
{
}

bool CSetupManager::IsEmpty()
{
	return startBoxes.empty();
}

CGameSetup::StartPosType CSetupManager::GetStartPosType()
{
	return startPosType;
}

const Box& CSetupManager::operator[](int idx) const
{
	return startBoxes[idx];
}

} // namespace circuit
