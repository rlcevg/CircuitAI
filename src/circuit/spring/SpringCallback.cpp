/*
 * SpringCallback.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#include "spring/SpringCallback.h"

#include "Unit.h"

namespace circuit {

using namespace springai;

COOAICallback::COOAICallback()
{
}

COOAICallback::~COOAICallback()
{
}

void COOAICallback::GetFriendlyUnits(std::vector<Unit*>& units) const
{
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
}

} // namespace circuit
