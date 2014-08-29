/*
 * StartBoxManager.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "StartBoxManager.h"

namespace circuit {

CStartBoxManager::CStartBoxManager(std::vector<Box>& startBoxes, CGameSetup::StartPosType startPosType) :
		startBoxes(startBoxes),
		startPosType(startPosType)
{
}

CStartBoxManager::~CStartBoxManager()
{
}

bool CStartBoxManager::IsEmpty()
{
	return startBoxes.empty();
}

CGameSetup::StartPosType CStartBoxManager::GetStartPosType()
{
	return startPosType;
}

const Box& CStartBoxManager::operator[](int idx) const
{
	return startBoxes[idx];
}

} // namespace circuit
