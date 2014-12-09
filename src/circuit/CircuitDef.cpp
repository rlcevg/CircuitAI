/*
 * CircuitDef.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "CircuitDef.h"
#include "utils.h"

namespace circuit {

using namespace springai;

CCircuitDef::CCircuitDef(std::unordered_set<springai::UnitDef*>& opts) :
		count(0),
		buildOptions(opts)
{
}

CCircuitDef::~CCircuitDef()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

const std::unordered_set<UnitDef*>& CCircuitDef::GetBuildOptions() const
{
	return buildOptions;
}

bool CCircuitDef::CanBuild(UnitDef* buildDef)
{
	return (buildOptions.find(buildDef) != buildOptions.end());
}

int CCircuitDef::GetCount()
{
	return count;
}

void CCircuitDef::Inc()
{
	count++;
}

void CCircuitDef::Dec()
{
	count--;
}

CCircuitDef& CCircuitDef::operator++ ()
{
	++count;
	return *this;
}

CCircuitDef  CCircuitDef::operator++ (int)
{
	CCircuitDef temp = *this;
	count++;
	return temp;
}

CCircuitDef& CCircuitDef::operator-- ()
{
	--count;
	return *this;
}

CCircuitDef  CCircuitDef::operator-- (int)
{
	CCircuitDef temp = *this;
	count--;
	return temp;
}

} // namespace circuit
