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

UnitDef* CCircuitDef::GetUnitDef() const
{
	return def;
}

const std::unordered_set<CCircuitDef::Id>& CCircuitDef::GetBuildOptions() const
{
	return buildOptions;
}

bool CCircuitDef::CanBuild(Id buildDefId) const
{
	return (buildOptions.find(buildDefId) != buildOptions.end());
}

bool CCircuitDef::CanBuild(CCircuitDef* buildDef) const
{
	return CanBuild(buildDef->GetId());
}

int CCircuitDef::GetCount() const
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

bool CCircuitDef::IsAvailable()
{
	return (def->GetMaxThisUnit() > count);
}

void CCircuitDef::IncBuild()
{
	buildCounts++;
}

void CCircuitDef::DecBuild()
{
	buildCounts--;
}

int CCircuitDef::GetBuildCount() const
{
	return buildCounts;
}

void CCircuitDef::SetImmobileId(STerrainMapImmobileType::Id immobileId)
{
	immobileTypeId = immobileId;
}

STerrainMapImmobileType::Id CCircuitDef::GetImmobileId() const
{
	return immobileTypeId;
}

void CCircuitDef::SetMobileId(STerrainMapMobileType::Id mobileId)
{
	mobileTypeId = mobileId;
}

STerrainMapMobileType::Id CCircuitDef::GetMobileId() const
{
	return mobileTypeId;
}

} // namespace circuit
