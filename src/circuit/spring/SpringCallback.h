/*
 * SpringCallback.h
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SPRING_SPRINGCALLBACK_H_
#define SRC_CIRCUIT_SPRING_SPRINGCALLBACK_H_

#include "OOAICallback.h"  // C++ wrapper

#include <vector>

struct SSkirmishAICallback;

namespace circuit {

class COOAICallback {
public:
	COOAICallback(springai::OOAICallback* clb);
	virtual ~COOAICallback();

	void Init(const struct SSkirmishAICallback* clb);

	springai::Debug*    GetDebug()    const { return callback->GetDebug(); }
	springai::DataDirs* GetDataDirs() const { return callback->GetDataDirs(); }
	springai::File*     GetFile()     const { return callback->GetFile(); }

	springai::Economy* GetEconomy() const { return callback->GetEconomy(); }
	springai::Map*     GetMap()     const { return callback->GetMap(); }
	springai::Mod*     GetMod()     const { return callback->GetMod(); }
	springai::Resource* GetResourceByName(const char* resourceName) const {
		return callback->GetResourceByName(resourceName);
	}

	std::vector<springai::Unit*> GetTeamUnits() const { return callback->GetTeamUnits(); }

//	void GetFriendlyUnits(std::vector<springai::Unit*>& units) const;
	std::vector<springai::Unit*> GetFriendlyUnits() const { return callback->GetFriendlyUnits(); }
	std::vector<springai::Unit*> GetFriendlyUnitsIn(const springai::AIFloat3& pos, float radius) const {
		return callback->GetFriendlyUnitsIn(pos, radius);
	}
	bool IsFriendlyUnitsIn(const springai::AIFloat3& pos, float radius) const;

	std::vector<springai::Unit*> GetEnemyUnits() const { return callback->GetEnemyUnits(); }
	std::vector<springai::Unit*> GetEnemyUnitsIn(const springai::AIFloat3& pos, float radius) const {
		return callback->GetEnemyUnitsIn(pos, radius);
	}

	std::vector<springai::Unit*> GetSelectedUnits() const { return callback->GetSelectedUnits(); }

	std::vector<springai::UnitDef*> GetUnitDefs() const { return callback->GetUnitDefs(); }
	std::vector<springai::WeaponDef*> GetWeaponDefs() const { return callback->GetWeaponDefs(); }

	std::vector<springai::Feature*> GetFeatures() const { return callback->GetFeatures(); }
	std::vector<springai::Feature*> GetFeaturesIn(const springai::AIFloat3& pos, float radius) const {
		return callback->GetFeaturesIn(pos, radius);
	}
	bool IsFeatures() const;
	bool IsFeaturesIn(const springai::AIFloat3& pos, float radius) const;

private:
	const struct SSkirmishAICallback* sAICallback;
	springai::OOAICallback* callback;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SPRING_SPRINGCALLBACK_H_
