/*
 * SpringCallback.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#include "spring/SpringCallback.h"

#include "util/Defines.h"

#include "SSkirmishAICallback.h"	// "direct" C API
#include "WrappUnit.h"

namespace circuit {

using namespace springai;

COOAICallback::COOAICallback(OOAICallback* clb)
		: sAICallback(nullptr)
		, callback(clb)
		, skirmishAIId(callback->GetSkirmishAIId())
{
	unitIds.resize(MAX_UNITS);
	units.resize(MAX_UNITS);
}

COOAICallback::~COOAICallback()
{
}

void COOAICallback::Init(const struct SSkirmishAICallback* clb)
{
	sAICallback = clb;
}

std::vector<Unit*> COOAICallback::GetFriendlyUnits()
{
	unitIds.resize(MAX_UNITS);
	int size = sAICallback->getFriendlyUnits(skirmishAIId, unitIds.data(), MAX_UNITS);

	units.resize(size);
	for (int i = 0; i < size; ++i) {
		units[i] = WrappUnit::GetInstance(skirmishAIId, unitIds[i]);
	}

	return units;
}

std::vector<Unit*> COOAICallback::GetFriendlyUnitsIn(const AIFloat3& pos, float radius)
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	unitIds.resize(MAX_UNITS);
	int size = sAICallback->getFriendlyUnitsIn(skirmishAIId, pos_posF3, radius, unitIds.data(), MAX_UNITS);

	units.resize(size);
	for (int i = 0; i < size; ++i) {
		units[i] = WrappUnit::GetInstance(skirmishAIId, unitIds[i]);
	}

	return units;
}

bool COOAICallback::IsFriendlyUnitsIn(const AIFloat3& pos, float radius) const
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	int size = sAICallback->getFriendlyUnitsIn(skirmishAIId, pos_posF3, radius, nullptr, -1);
	return size > 0;
}

std::vector<int> COOAICallback::GetFriendlyUnitIdsIn(const springai::AIFloat3& pos, float radius)
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	unitIds.resize(MAX_UNITS);
	int size = sAICallback->getFriendlyUnitsIn(skirmishAIId, pos_posF3, radius, unitIds.data(), MAX_UNITS);
	unitIds.resize(size);
	return unitIds;
}

std::vector<Unit*> COOAICallback::GetEnemyUnits()
{
	unitIds.resize(MAX_UNITS);
	int size = sAICallback->getEnemyUnits(skirmishAIId, unitIds.data(), MAX_UNITS);

	units.resize(size);
	for (int i = 0; i < size; ++i) {
		units[i] = WrappUnit::GetInstance(skirmishAIId, unitIds[i]);
	}

	return units;
}

std::vector<Unit*> COOAICallback::GetEnemyUnitsIn(const AIFloat3& pos, float radius)
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	unitIds.resize(MAX_UNITS);
	int size = sAICallback->getEnemyUnitsIn(skirmishAIId, pos_posF3, radius, unitIds.data(), MAX_UNITS);

	units.resize(size);
	for (int i = 0; i < size; ++i) {
		units[i] = WrappUnit::GetInstance(skirmishAIId, unitIds[i]);
	}

	return units;
}

std::vector<int> COOAICallback::GetEnemyUnitIdsIn(const AIFloat3& pos, float radius)
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	unitIds.resize(MAX_UNITS);
	int size = sAICallback->getEnemyUnitsIn(skirmishAIId, pos_posF3, radius, unitIds.data(), MAX_UNITS);
	unitIds.resize(size);
	return unitIds;
}

bool COOAICallback::IsFeatures() const
{
	int size = sAICallback->getFeatures(skirmishAIId, nullptr, -1);
	return size > 0;
}

bool COOAICallback::IsFeaturesIn(const AIFloat3& pos, float radius) const
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	int size = sAICallback->getFeaturesIn(skirmishAIId, pos_posF3, radius, nullptr, -1);
	return size > 0;
}

int COOAICallback::Unit_GetDefId(int unitId) const
{
	return sAICallback->Unit_getDef(skirmishAIId, unitId);
}

bool COOAICallback::Unit_hasCommands(int unitId) const
{
	return sAICallback->Unit_getCurrentCommands(skirmishAIId, unitId) > 0;
}

} // namespace circuit
