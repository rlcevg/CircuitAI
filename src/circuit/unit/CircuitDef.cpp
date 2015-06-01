/*
 * CircuitDef.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitDef.h"
#include "util/utils.h"

#include "WeaponMount.h"
#include "WeaponDef.h"

namespace circuit {

using namespace springai;

CCircuitDef::CCircuitDef(springai::UnitDef* def, std::unordered_set<Id>& buildOpts) :
		id(def->GetUnitDefId()),
		def(def),
		count(0),
		buildOptions(buildOpts),
		buildDistance(def->GetBuildDistance()),
		buildCounts(0),
		reloadFrames(-1),
		dgunRange(.0f),
		mobileTypeId(-1),
		immobileTypeId(-1)
{
	if (def->CanManualFire()) {
		float bestReload = std::numeric_limits<float>::max();
		float bestRange;
		auto mounts = std::move(def->GetWeaponMounts());
		for (WeaponMount* mount : mounts) {
			WeaponDef* wd = mount->GetWeaponDef();
			float reload;
			if (wd->IsManualFire() && ((reload = wd->GetReload()) < bestReload)) {
				bestReload = reload;
				bestRange = wd->GetRange();
			}
			delete wd;
		}
		utils::free_clear(mounts);
		if (bestReload < std::numeric_limits<float>::max()) {
			reloadFrames = math::ceil(bestReload * FRAMES_PER_SEC) + FRAMES_PER_SEC;
			dgunRange = bestRange;
		}
	}
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
