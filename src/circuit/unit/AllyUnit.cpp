/*
 * AllyUnit.cpp
 *
 *  Created on: Nov 10, 2017
 *      Author: rlcevg
 */

#include "unit/AllyUnit.h"
#include "terrain/TerrainManager.h"
#include "util/utils.h"

namespace circuit {

using namespace springai;

CAllyUnit::CAllyUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef)
		: ICoreUnit(unitId, unit, cdef)
		, task(nullptr)
		, posFrame(-1)
{
}

CAllyUnit::~CAllyUnit()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

const AIFloat3& CAllyUnit::GetPos(int frame)
{
	if (posFrame != frame) {
		posFrame = frame;
		position = unit->GetPos();
		CTerrainManager::CorrectPosition(position);
	}
	return position;
}

} // namespace circuit
