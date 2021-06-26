/*
 * EnergyData.cpp
 *
 *  Created on: Jun 25, 2021
 *      Author: rlcevg
 */

#include "resource/EnergyData.h"

namespace circuit {

CEnergyData::CEnergyData()
		: isInitialized(false)
{
}

CEnergyData::~CEnergyData()
{
}

void CEnergyData::Init(const Geos&& spots)
{
	this->spots = spots;
}

} // namespace circuit
