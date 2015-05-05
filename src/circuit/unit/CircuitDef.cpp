/*
 * CircuitDef.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitDef.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CCircuitDef::CCircuitDef(springai::UnitDef* def, std::unordered_set<Id>& buildOpts) :
		id(def->GetUnitDefId()),
		def(def),
		count(0),
		buildOptions(buildOpts),
		buildDistance(def->GetBuildDistance()),
		buildCounts(0),
		mobileTypeId(-1),
		immobileTypeId(-1)
{
}

CCircuitDef::~CCircuitDef()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete def;
}

CCircuitDef& CCircuitDef::operator++()
{
	++count;
	return *this;
}

CCircuitDef CCircuitDef::operator++(int)
{
	CCircuitDef temp = *this;
	count++;
	return temp;
}

CCircuitDef& CCircuitDef::operator--()
{
	--count;
	return *this;
}

CCircuitDef CCircuitDef::operator--(int)
{
	CCircuitDef temp = *this;
	count--;
	return temp;
}

} // namespace circuit
