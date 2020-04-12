/*
 * AllyUnit.cpp
 *
 *  Created on: Nov 10, 2017
 *      Author: rlcevg
 */

#include "unit/ally/AllyUnit.h"
#include "util/Utils.h"
#include "terrain/TerrainData.h"

namespace circuit {

using namespace springai;

CAllyUnit::CAllyUnit(Id unitId, springai::Unit* unit, CCircuitDef* cdef)
		: ICoreUnit(unitId, unit)
		, circuitDef(cdef)
		, task(nullptr)
		, posFrame(-1)
{
}

CAllyUnit::~CAllyUnit()
{
}

const AIFloat3& CAllyUnit::GetPos(int frame)
{
	if (posFrame != frame) {
		posFrame = frame;
		position = unit->GetPos();
		CTerrainData::CorrectPosition(position);
	}
	return position;
}

} // namespace circuit
