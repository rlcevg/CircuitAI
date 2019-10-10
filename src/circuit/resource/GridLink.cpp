/*
 * GridLink.cpp
 *
 *  Created on: Oct 8, 2019
 *      Author: rlcevg
 */

#include "resource/GridLink.h"

namespace circuit {

IGridLink::IGridLink()
		: isBeingBuilt(false)
		, isFinished(false)
		, isValid(true)
{
}

IGridLink::~IGridLink()
{
}

} // namespace circuit
