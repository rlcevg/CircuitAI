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

CCircuitDef::CCircuitDef(CCircuitAI* circuit, UnitDef* def, std::unordered_set<Id>& buildOpts, Resource* res)
		: id(def->GetUnitDefId())
		, def(def)
		, count(0)
		, buildOptions(buildOpts)
		, buildDistance(def->GetBuildDistance())
		, buildCounts(0)
//		, dgunReload(-1)
		, dgunRange(.0f)
		, dgunMount(nullptr)
		, mobileTypeId(-1)
		, immobileTypeId(-1)
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
	bool isTracks = false;
	bool isWater = false;
	auto mounts = std::move(def->GetWeaponMounts());
	for (WeaponMount* mount : mounts) {
		WeaponDef* wd = mount->GetWeaponDef();
		const std::map<std::string, std::string>& customParams = wd->GetCustomParams();

		float scale = wd->IsParalyzer() ? 0.2f : 1.0f;

		float extraDmg = .0f;
		auto it = customParams.find("extra_damage");
		if (it != customParams.end()) {
			extraDmg = utils::string_to_float(it->second);
		}

		it = customParams.find("disarmdamageonly");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 0.5f;
		}

		it = customParams.find("timeslow_onlyslow");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 0.5f;
		}

		it = customParams.find("is_capture");
		if ((it != customParams.end()) && (utils::string_to_int(it->second) == 1)) {
			scale = 2.0f;
		}

		float reloadTime = wd->GetReload();
		if (extraDmg > 0.1f) {
			dps += extraDmg * wd->GetSalvoSize() / reloadTime;
		}
		Damage* damage = wd->GetDamage();
		const std::vector<float>& damages = damage->GetTypes();
		delete damage;
		float ldps = .0f;
		for (float dmg : damages) {
			ldps += dmg;
		}
		dps += ldps * wd->GetSalvoSize() * scale / damages.size() / reloadTime;
		targetCategory |= mount->GetOnlyTargetCategory();
		isTracks |= (wd->GetProjectileSpeed() * FRAMES_PER_SEC >= 500.0f) || wd->IsTracks();
		isWater |= wd->IsWaterWeapon();

		delete wd;
		delete mount;
	}
	isAntiAir   = (targetCategory & circuit->GetAirCategory()) && isTracks;
	isAntiLand  = (targetCategory & circuit->GetLandCategory());
	isAntiWater = (targetCategory & circuit->GetWaterCategory()) || isWater;

	isMobile = def->GetSpeed() > .0f;

	cost = def->GetCost(res);
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
