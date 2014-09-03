/*
 * MetalManager.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "MetalManager.h"

namespace circuit {

using namespace springai;

CMetalManager::CMetalManager(std::vector<Metal>& spots) :
		spots(spots),
		mexPerClusterAvg(DEFAULT_MEXPERCLUSTER)
{
}

CMetalManager::~CMetalManager()
{
}

bool CMetalManager::IsEmpty()
{
	return spots.empty();
}

std::vector<Metal>& CMetalManager::GetSpots()
{
	return spots;
}

} // namespace circuit
