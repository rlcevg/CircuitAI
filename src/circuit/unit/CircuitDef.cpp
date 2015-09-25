/*
 * CircuitDef.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "unit/CircuitDef.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "WeaponMount.h"
#include "WeaponDef.h"
#include "Damage.h"

namespace circuit {

using namespace springai;

CCircuitDef::CCircuitDef(UnitDef* def, std::unordered_set<Id>& buildOpts, CCircuitAI* circuit) :
		id(def->GetUnitDefId()),
		def(def),
		count(0),
		buildOptions(buildOpts),
		buildDistance(def->GetBuildDistance()),
		buildCounts(0),
//		dgunReload(-1),
		dgunRange(.0f),
		dgunMount(nullptr),
		mobileTypeId(-1),
		immobileTypeId(-1)
{
	if (def->CanManualFire()) {
		float bestReload = std::numeric_limits<float>::max();
		float bestRange;
		WeaponMount* bestMount = nullptr;
		auto mounts = std::move(def->GetWeaponMounts());
		for (WeaponMount* mount : mounts) {
			WeaponDef* wd = mount->GetWeaponDef();
			float reload;
			if (wd->IsManualFire() && ((reload = wd->GetReload()) < bestReload)) {
				bestReload = reload;
				bestRange = wd->GetRange();
				delete bestMount;
				bestMount = mount;
			} else {
				delete mount;
			}
			delete wd;
		}
		if (bestReload < std::numeric_limits<float>::max()) {
//			dgunReload = math::ceil(bestReload * FRAMES_PER_SEC)/* + FRAMES_PER_SEC*/;
			dgunRange = bestRange;
			dgunMount = bestMount;
		}
	}

	dps = 0.0f;
	int targetCategory = 0;
	auto mounts = std::move(def->GetWeaponMounts());
	for (WeaponMount* mount : mounts) {
		WeaponDef* wd = mount->GetWeaponDef();
		const std::map<std::string, std::string>& customParams = wd->GetCustomParams();

		bool valid = !wd->IsParalyzer();

		float extraDmg = .0f;
		auto it = customParams.find("extra_damage");
		if (it != customParams.end()) {
			extraDmg = utils::string_to_float(it->second);
			valid |= extraDmg > 0.1f;
		}

		it = customParams.find("disarmdamageonly");
		if (it != customParams.end()) {
			valid |= utils::string_to_int(it->second) == 0;
		}

		it = customParams.find("timeslow_onlyslow");
		if (it != customParams.end()) {
			valid |= utils::string_to_int(it->second) == 0;
		}

		if (valid) {
			float reloadTime = wd->GetReload();
			if (extraDmg > 0.1f) {
				dps += extraDmg * wd->GetSalvoSize() / reloadTime;
			} else {
				Damage* damage = wd->GetDamage();
				const std::vector<float>& damages = damage->GetTypes();
				delete damage;
				float ldps = .0f;
				for (float dmg : damages) {
					ldps += dmg;
				}
				dps += ldps * wd->GetSalvoSize() / damages.size() / reloadTime;
			}
			targetCategory |= mount->GetOnlyTargetCategory();
		}
		delete wd;
		delete mount;
	}
	isAntiAir   = (targetCategory & circuit->GetAirCategory());
	isAntiLand  = (targetCategory & circuit->GetLandCategory());
	isAntiWater = (targetCategory & circuit->GetWaterCategory());
}

CCircuitDef::~CCircuitDef()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete def, dgunMount;
}

CCircuitDef& CCircuitDef::operator++()
{
	++count;
	return *this;
}

// FIXME: ~CCircuitDef should fail with delete
//CCircuitDef CCircuitDef::operator++(int)
//{
//	CCircuitDef temp = *this;
//	count++;
//	return temp;
//}

CCircuitDef& CCircuitDef::operator--()
{
	--count;
	return *this;
}

//CCircuitDef CCircuitDef::operator--(int)
//{
//	CCircuitDef temp = *this;
//	count--;
//	return temp;
//}

} // namespace circuit
