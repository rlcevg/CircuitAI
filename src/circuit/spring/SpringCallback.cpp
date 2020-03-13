/*
 * SpringCallback.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#include "spring/SpringCallback.h"

#include "SSkirmishAICallback.h"	// "direct" C API
//#include "WrappUnit.h"

namespace circuit {

using namespace springai;

COOAICallback::COOAICallback(OOAICallback* clb)
		: sAICallback(nullptr)
		, callback(clb)
{
}

COOAICallback::~COOAICallback()
{
}

void COOAICallback::Init(const struct SSkirmishAICallback* clb)
{
	sAICallback = clb;
}

//void COOAICallback::GetFriendlyUnits(std::vector<Unit*>& units) const
//{
//	int unitIds_sizeMax;
//	int unitIds_raw_size;
//	int* unitIds;
//	std::vector<springai::Unit*> unitIds_list;
//	int unitIds_size;
//	int internal_ret_int;
//
//	unitIds_sizeMax = INT_MAX;
//	unitIds = NULL;
//	unitIds_size = bridged_getFriendlyUnits(this->GetSkirmishAIId(), unitIds, unitIds_sizeMax);
//	unitIds_sizeMax = unitIds_size;
//	unitIds_raw_size = unitIds_size;
//	unitIds = new int[unitIds_raw_size];
//
//	internal_ret_int = bridged_getFriendlyUnits(this->GetSkirmishAIId(), unitIds, unitIds_sizeMax);
//	unitIds_list.reserve(unitIds_size);
//	for (int i=0; i < unitIds_sizeMax; ++i) {
//		unitIds_list.push_back(springai::WrappUnit::GetInstance(skirmishAIId, unitIds[i]));
//	}
//	delete[] unitIds;
//
//	return unitIds_list;
//}

bool COOAICallback::IsFriendlyUnitsIn(const AIFloat3& pos, float radius) const
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	int size = sAICallback->getFriendlyUnitsIn(callback->GetSkirmishAIId(), pos_posF3, radius, nullptr, -1);
	return size > 0;
}

bool COOAICallback::IsFeatures() const
{
	int size = sAICallback->getFeatures(callback->GetSkirmishAIId(), nullptr, -1);
	return size > 0;
}

bool COOAICallback::IsFeaturesIn(const AIFloat3& pos, float radius) const
{
	float pos_posF3[3];
	pos.LoadInto(pos_posF3);
	int size = sAICallback->getFeaturesIn(callback->GetSkirmishAIId(), pos_posF3, radius, nullptr, -1);
	return size > 0;
}

} // namespace circuit
