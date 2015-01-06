/*
 * Squad.cpp
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 */

#include "unit/Squad.h"
#include "util/utils.h"

namespace circuit {

CSquad::CSquad()
{
	// TODO Auto-generated constructor stub

}

CSquad::~CSquad()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

std::set<CCircuitUnit*>& CSquad::GetAssignees()
{
	return units;
}

} // namespace circuit
