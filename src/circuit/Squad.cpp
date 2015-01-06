/*
 * Squad.cpp
 *
 *  Created on: Jan 5, 2015
 *      Author: rlcevg
 */

#include "Squad.h"
#include "utils.h"

namespace circuit {

CSquad::CSquad()
{
	// TODO Auto-generated constructor stub

}

CSquad::~CSquad()
{
	// TODO Auto-generated destructor stub
}

std::set<CCircuitUnit*>& CSquad::GetAssignees()
{
	return units;
}

} // namespace circuit
