/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "util/math/QuadField.h"
#include "util/Container.h"
#include "util/Utils.h"
#include "unit/ally/AllyUnit.h"
#include "unit/enemy/EnemyUnit.h"

namespace circuit {
/*
void CQuadField::Resize(int quad_size)
{
	CQuadField* oldQuadField = &quadField;
	CQuadField newQuadField;

	newQuadField.Init(int2(mapDims.mapx, mapDims.mapy), quad_size);

	for (int zq = 0; zq < oldQuadField->GetNumQuadsZ(); zq++) {
		for (int xq = 0; xq < oldQuadField->GetNumQuadsX(); xq++) {
			const CQuadField::Quad& quad = oldQuadField->GetQuadAt(xq, zq);

			// COPY the object lists because the Remove* functions modify them
			// NOTE:
			//   teamUnits is updated internally by RemoveUnit and MovedUnit
			//
			//   if a unit exists in multiple quads in the old field, it will
			//   be removed from all of them and there is no danger of double
			//   re-insertion (important if new grid has higher resolution)
			const std::vector<CUnit*      > units       = quad.units;
			const std::vector<CFeature*   > features    = quad.features;
			const std::vector<CProjectile*> projectiles = quad.projectiles;

			for (auto it = units.cbegin(); it != units.cend(); ++it) {
				oldQuadField->RemoveUnit(*it);
				newQuadField->MovedUnit(*it); // handles addition
			}

			for (auto it = features.cbegin(); it != features.cend(); ++it) {
				oldQuadField->RemoveFeature(*it);
				newQuadField->AddFeature(*it);
			}

			for (auto it = projectiles.cbegin(); it != projectiles.cend(); ++it) {
				oldQuadField->RemoveProjectile(*it);
				newQuadField->AddProjectile(*it);
			}
		}
	}

	quadField = std::move(newQuadField);
}
*/
void CQuadField::Init(int2 mapDims, int quadSize)
{
	quadSizeX = quadSize;
	quadSizeZ = quadSize;
	numQuadsX = (mapDims.x * SQUARE_SIZE) / quadSize;
	numQuadsZ = (mapDims.y * SQUARE_SIZE) / quadSize;

	assert(numQuadsX >= 1);
	assert(numQuadsZ >= 1);
	assert((mapDims.x * SQUARE_SIZE) % quadSize == 0);
	assert((mapDims.y * SQUARE_SIZE) % quadSize == 0);

	invQuadSize = {1.0f / quadSizeX, 1.0f / quadSizeZ};

	baseQuads.resize(numQuadsX * numQuadsZ);
	tempQuads.ReserveAll(numQuadsX * numQuadsZ);
	tempQuads.ReleaseAll();
}

void CQuadField::Kill()
{
	// reuse quads when reloading
	// baseQuads.clear();
	for (Quad& quad: baseQuads) {
		quad.Clear();
	}

	tempAllyUnits.ReleaseAll();
	tempEnemyUnits.ReleaseAll();
	tempEnemyFakes.ReleaseAll();
	tempQuads.ReleaseAll();
}

int2 CQuadField::WorldPosToQuadField(const AIFloat3 p) const
{
	return int2(
		utils::clamp(int(p.x / quadSizeX), 0, numQuadsX - 1),
		utils::clamp(int(p.z / quadSizeZ), 0, numQuadsZ - 1)
	);
}

int CQuadField::WorldPosToQuadFieldIdx(const AIFloat3 p) const
{
	return utils::clamp(int(p.z / quadSizeZ), 0, numQuadsZ - 1) * numQuadsX + utils::clamp(int(p.x / quadSizeX), 0, numQuadsX - 1);
}

void CQuadField::GetQuads(QuadFieldQuery& qfq, AIFloat3 pos, float radius)
{
	pos.AssertNaNs();
	pos.ClampInBounds();
	qfq.quads = tempQuads.ReserveVector();

	const int2 min = WorldPosToQuadField(pos - radius);
	const int2 max = WorldPosToQuadField(pos + radius);

	if (max.y < min.y || max.x < min.x)
		return;

	// qsx and qsz are always equal
	const float maxSqLength = (radius + quadSizeX * 0.72f) * (radius + quadSizeZ * 0.72f);

	for (int z = min.y; z <= max.y; ++z) {
		for (int x = min.x; x <= max.x; ++x) {
			assert(x < numQuadsX);
			assert(z < numQuadsZ);
			const AIFloat3 quadPos = AIFloat3(x * quadSizeX + quadSizeX * 0.5f, 0, z * quadSizeZ + quadSizeZ * 0.5f);
			if (pos.SqDistance2D(quadPos) < maxSqLength) {
				qfq.quads->push_back(z * numQuadsX + x);
			}
		}
	}

	return;
}

void CQuadField::GetQuadsRectangle(QuadFieldQuery& qfq, const AIFloat3& mins, const AIFloat3& maxs)
{
	mins.AssertNaNs();
	maxs.AssertNaNs();
	qfq.quads = tempQuads.ReserveVector();

	const int2 min = WorldPosToQuadField(mins);
	const int2 max = WorldPosToQuadField(maxs);

	if (max.y < min.y || max.x < min.x)
		return;

	for (int z = min.y; z <= max.y; ++z) {
		for (int x = min.x; x <= max.x; ++x) {
			assert(x < numQuadsX);
			assert(z < numQuadsZ);
			qfq.quads->push_back(z * numQuadsX + x);
		}
	}

	return;
}

/// note: this function got an UnitTest, check the tests/ folder!
void CQuadField::GetQuadsOnRay(QuadFieldQuery& qfq, const AIFloat3& start, const AIFloat3& dir, float length)
{
	dir.AssertNaNs();
	start.AssertNaNs();

	auto& queryQuads = *(qfq.quads = tempQuads.ReserveVector());

	const AIFloat3 to = start + (dir * length);

	const bool noXdir = (math::floor(start.x * invQuadSize.x) == math::floor(to.x * invQuadSize.x));
	const bool noZdir = (math::floor(start.z * invQuadSize.y) == math::floor(to.z * invQuadSize.y));


	// special case
	if (noXdir && noZdir) {
		queryQuads.push_back(WorldPosToQuadFieldIdx(start));
		assert(static_cast<unsigned>(queryQuads.back()) < baseQuads.size());
		return;
	}

	// prevent div0
	if (noZdir) {
		int startX = utils::clamp<int>(start.x * invQuadSize.x, 0, numQuadsX - 1);
		int finalX = utils::clamp<int>(   to.x * invQuadSize.x, 0, numQuadsX - 1);

		if (finalX < startX)
			std::swap(startX, finalX);

		assert(finalX < numQuadsX);

		const int row = utils::clamp<int>(start.z * invQuadSize.y, 0, numQuadsZ - 1) * numQuadsX;

		for (int x = startX; x <= finalX; x++) {
			queryQuads.push_back(row + x);
			assert(static_cast<unsigned>(queryQuads.back()) < baseQuads.size());
		}

		return;
	}


	// iterate z-range; compute which columns (x) are touched for each row (z)
	float startZuc = start.z * invQuadSize.y;
	float finalZuc =    to.z * invQuadSize.y;

	if (finalZuc < startZuc)
		std::swap(startZuc, finalZuc);

	const int startZ = utils::clamp<int>(startZuc, 0, numQuadsZ - 1);
	const int finalZ = utils::clamp<int>(finalZuc, 0, numQuadsZ - 1);

	assert(finalZ < quadSizeZ);

	const float invDirZ = 1.0f / dir.z;

	for (int z = startZ; z <= finalZ; z++) {
		float t0 = ((z    ) * quadSizeZ - start.z) * invDirZ;
		float t1 = ((z + 1) * quadSizeZ - start.z) * invDirZ;

		if ((startZuc < 0 && z == 0) || (startZuc >= numQuadsZ && z == finalZ))
			t0 = ((startZuc    ) * quadSizeZ - start.z) * invDirZ;

		if ((finalZuc < 0 && z == 0) || (finalZuc >= numQuadsZ && z == finalZ))
			t1 = ((finalZuc + 1) * quadSizeZ - start.z) * invDirZ;

		t0 = utils::clamp(t0, 0.0f, length);
		t1 = utils::clamp(t1, 0.0f, length);

		unsigned startX = utils::clamp<int>((dir.x * t0 + start.x) * invQuadSize.x, 0, numQuadsX - 1);
		unsigned finalX = utils::clamp<int>((dir.x * t1 + start.x) * invQuadSize.x, 0, numQuadsX - 1);

		if (finalX < startX)
			std::swap(startX, finalX);

		assert(finalX < numQuadsX);

		const int row = utils::clamp(z, 0, numQuadsZ - 1) * numQuadsX;

		for (unsigned x = startX; x <= finalX; x++) {
			queryQuads.push_back(row + x);
			assert(static_cast<unsigned>(queryQuads.back()) < baseQuads.size());
		}
	}
}

bool CQuadField::InsertAllyUnitIf(CAllyUnit* unit, const AIFloat3& upos, const AIFloat3& wpos)
{
	assert(unit != nullptr);

	const int wposQuadIdx = WorldPosToQuadFieldIdx(wpos);
	const int uposQuadIdx = WorldPosToQuadFieldIdx(upos);  // unit->pos

	// do nothing if unit already exists in cell containing <wpos>
	if (wposQuadIdx == uposQuadIdx)
		return false;

	// unit might also be overlapping the cell, so test for uniqueness
	if (!utils::VectorInsertUnique(unit->quads, wposQuadIdx, true))
		return false;

	utils::VectorInsertUnique(baseQuads[wposQuadIdx].allyUnits, unit, false);
	return true;
}

bool CQuadField::RemoveAllyUnitIf(CAllyUnit* unit, const AIFloat3& upos, const AIFloat3& wpos)
{
	if (unit == nullptr)
		return false;

	const int wposQuadIdx = WorldPosToQuadFieldIdx(wpos);
	const int uposQuadIdx = WorldPosToQuadFieldIdx(upos);  // unit->pos

	// do nothing if unit now exists in cell containing <wpos>
	// (meaning it must have somehow moved since InsertUnitIf)
	if (wposQuadIdx == uposQuadIdx)
		return false;

	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, upos, unit->GetCircuitDef()->GetRadius());

	// do nothing if the cells touched by unit now contain <wpos>
	if (std::find(qfQuery.quads->begin(), qfQuery.quads->end(), wposQuadIdx) != qfQuery.quads->end()) {
		assert(std::find(unit->quads.begin(), unit->quads.end(), wposQuadIdx) != unit->quads.end());
		return false;
	}

	if (!utils::VectorErase(unit->quads, wposQuadIdx))
		return false;

	utils::VectorErase(baseQuads[wposQuadIdx].allyUnits, unit);
	return true;
}

void CQuadField::MovedAllyUnit(CAllyUnit* unit, const AIFloat3& upos)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, upos, unit->GetCircuitDef()->GetRadius());

	// compare if the quads have changed, if not stop here
	if (qfQuery.quads->size() == unit->quads.size()) {
		if (std::equal(qfQuery.quads->begin(), qfQuery.quads->end(), unit->quads.begin()))
			return;
	}

	for (const int qi: unit->quads) {
		utils::VectorErase(baseQuads[qi].allyUnits, unit);
	}

	for (const int qi: *qfQuery.quads) {
		utils::VectorInsertUnique(baseQuads[qi].allyUnits, unit, false);
	}

	unit->quads = std::move(*qfQuery.quads);
}

void CQuadField::RemoveAllyUnit(CAllyUnit* unit, const AIFloat3& upos)
{
	for (const int qi: unit->quads) {
		utils::VectorErase(baseQuads[qi].allyUnits, unit);
	}

	unit->quads.clear();
}

bool CQuadField::InsertEnemyUnitIf(CEnemyUnit* unit, const AIFloat3& wpos)
{
	assert(unit != nullptr);

	const int wposQuadIdx = WorldPosToQuadFieldIdx(wpos);
	const int uposQuadIdx = WorldPosToQuadFieldIdx(unit->GetPos());

	// do nothing if unit already exists in cell containing <wpos>
	if (wposQuadIdx == uposQuadIdx)
		return false;

	// unit might also be overlapping the cell, so test for uniqueness
	if (!utils::VectorInsertUnique(unit->quads, wposQuadIdx, true))
		return false;

	utils::VectorInsertUnique(baseQuads[wposQuadIdx].enemyUnits, unit, false);
	return true;
}

bool CQuadField::RemoveEnemyUnitIf(CEnemyUnit* unit, const AIFloat3& wpos)
{
	if (unit == nullptr)
		return false;

	const int wposQuadIdx = WorldPosToQuadFieldIdx(wpos);
	const int uposQuadIdx = WorldPosToQuadFieldIdx(unit->GetPos());

	// do nothing if unit now exists in cell containing <wpos>
	// (meaning it must have somehow moved since InsertUnitIf)
	if (wposQuadIdx == uposQuadIdx)
		return false;

	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, unit->GetPos(), unit->GetRadius());

	// do nothing if the cells touched by unit now contain <wpos>
	if (std::find(qfQuery.quads->begin(), qfQuery.quads->end(), wposQuadIdx) != qfQuery.quads->end()) {
		assert(std::find(unit->quads.begin(), unit->quads.end(), wposQuadIdx) != unit->quads.end());
		return false;
	}

	if (!utils::VectorErase(unit->quads, wposQuadIdx))
		return false;

	utils::VectorErase(baseQuads[wposQuadIdx].enemyUnits, unit);
	return true;
}

void CQuadField::MovedEnemyUnit(CEnemyUnit* unit)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, unit->GetPos(), unit->GetRadius());

	// compare if the quads have changed, if not stop here
	if (qfQuery.quads->size() == unit->quads.size()) {
		if (std::equal(qfQuery.quads->begin(), qfQuery.quads->end(), unit->quads.begin()))
			return;
	}

	for (const int qi: unit->quads) {
		utils::VectorErase(baseQuads[qi].enemyUnits, unit);
	}

	for (const int qi: *qfQuery.quads) {
		utils::VectorInsertUnique(baseQuads[qi].enemyUnits, unit, false);
	}

	unit->quads = std::move(*qfQuery.quads);
}

void CQuadField::RemoveEnemyUnit(CEnemyUnit* unit)
{
	for (const int qi: unit->quads) {
		utils::VectorErase(baseQuads[qi].enemyUnits, unit);
	}

	unit->quads.clear();
}

void CQuadField::MovedEnemyFake(CEnemyUnit* unit)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, unit->GetPos(), unit->GetRadius());

	// compare if the quads have changed, if not stop here
	if (qfQuery.quads->size() == unit->quads.size()) {
		if (std::equal(qfQuery.quads->begin(), qfQuery.quads->end(), unit->quads.begin()))
			return;
	}

	for (const int qi: unit->quads) {
		utils::VectorErase(baseQuads[qi].enemyFakes, unit);
	}

	for (const int qi: *qfQuery.quads) {
		utils::VectorInsertUnique(baseQuads[qi].enemyFakes, unit, false);
	}

	unit->quads = std::move(*qfQuery.quads);
}

void CQuadField::RemoveEnemyFake(CEnemyUnit* unit)
{
	for (const int qi: unit->quads) {
		utils::VectorErase(baseQuads[qi].enemyFakes, unit);
	}

	unit->quads.clear();
}

void CQuadField::GetAllyUnits(QuadFieldQuery& qfq, const AIFloat3& pos, float radius)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, pos, radius);
	const int tempNum = GetTempNum();
	qfq.allyUnits = tempAllyUnits.ReserveVector();

	for (const int qi: *qfQuery.quads) {
		for (CAllyUnit* u: baseQuads[qi].allyUnits) {
			if (u->tempNum == tempNum)
				continue;

			u->tempNum = tempNum;
			qfq.allyUnits->push_back(u);
		}
	}

	return;
}

void CQuadField::GetAllyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& pos, float radius, bool spherical)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, pos, radius);
	const int tempNum = GetTempNum();
	qfq.allyUnits = tempAllyUnits.ReserveVector();

	for (const int qi: *qfQuery.quads) {
		for (CAllyUnit* u: baseQuads[qi].allyUnits) {
			if (u->tempNum == tempNum)
				continue;

			u->tempNum = tempNum;

			const float totRad       = radius + u->GetCircuitDef()->GetRadius();
			const float totRadSq     = totRad * totRad;
			const float posUnitDstSq = spherical?
				pos.SqDistance(u->GetLastPos()):
				pos.SqDistance2D(u->GetLastPos());

			if (posUnitDstSq >= totRadSq)
				continue;

			qfq.allyUnits->push_back(u);
		}
	}

	return;
}

void CQuadField::GetAllyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& mins, const AIFloat3& maxs)
{
	QuadFieldQuery qfQuery(*this);
	GetQuadsRectangle(qfQuery, mins, maxs);
	const int tempNum = GetTempNum();
	qfq.allyUnits = tempAllyUnits.ReserveVector();

	for (const int qi: *qfQuery.quads) {
		for (CAllyUnit* unit: baseQuads[qi].allyUnits) {

			if (unit->tempNum == tempNum)
				continue;

			unit->tempNum = tempNum;

			const AIFloat3& pos = unit->GetLastPos();
			if (pos.x < mins.x || pos.x > maxs.x)
				continue;
			if (pos.z < mins.z || pos.z > maxs.z)
				continue;

			qfq.allyUnits->push_back(unit);
		}
	}

	return;
}

void CQuadField::GetEnemyUnits(QuadFieldQuery& qfq, const AIFloat3& pos, float radius)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, pos, radius);
	const int tempNum = GetTempNum();
	qfq.enemyUnits = tempEnemyUnits.ReserveVector();

	for (const int qi: *qfQuery.quads) {
		for (CEnemyUnit* u: baseQuads[qi].enemyUnits) {
			if (u->tempNum == tempNum)
				continue;

			u->tempNum = tempNum;
			qfq.enemyUnits->push_back(u);
		}
	}

	return;
}

void CQuadField::GetEnemyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& pos, float radius, bool spherical)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, pos, radius);
	const int tempNum = GetTempNum();
	qfq.enemyUnits = tempEnemyUnits.ReserveVector();

	for (const int qi: *qfQuery.quads) {
		for (CEnemyUnit* u: baseQuads[qi].enemyUnits) {
			if (u->tempNum == tempNum)
				continue;

			u->tempNum = tempNum;

			const float totRad       = radius + u->GetRadius();
			const float totRadSq     = totRad * totRad;
			const float posUnitDstSq = spherical?
				pos.SqDistance(u->GetPos()):
				pos.SqDistance2D(u->GetPos());

			if (posUnitDstSq >= totRadSq)
				continue;

			qfq.enemyUnits->push_back(u);
		}
	}

	return;
}

void CQuadField::GetEnemyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& mins, const AIFloat3& maxs)
{
	QuadFieldQuery qfQuery(*this);
	GetQuadsRectangle(qfQuery, mins, maxs);
	const int tempNum = GetTempNum();
	qfq.enemyUnits = tempEnemyUnits.ReserveVector();

	for (const int qi: *qfQuery.quads) {
		for (CEnemyUnit* unit: baseQuads[qi].enemyUnits) {

			if (unit->tempNum == tempNum)
				continue;

			unit->tempNum = tempNum;

			const AIFloat3& pos = unit->GetPos();
			if (pos.x < mins.x || pos.x > maxs.x)
				continue;
			if (pos.z < mins.z || pos.z > maxs.z)
				continue;

			qfq.enemyUnits->push_back(unit);
		}
	}

	return;
}

void CQuadField::GetEnemyAndFakes(QuadFieldQuery& qfq, const AIFloat3& pos, float radius)
{
	QuadFieldQuery qfQuery(*this);
	GetQuads(qfQuery, pos, radius);
	const int tempNum = GetTempNum();
	qfq.enemyUnits = tempEnemyUnits.ReserveVector();
	qfq.enemyFakes = tempEnemyFakes.ReserveVector();

	for (const int qi: *qfQuery.quads) {
		for (CEnemyUnit* u: baseQuads[qi].enemyUnits) {
			if (u->tempNum == tempNum)
				continue;

			u->tempNum = tempNum;
			qfq.enemyUnits->push_back(u);
		}
		for (CEnemyUnit* u: baseQuads[qi].enemyFakes) {
			if (u->tempNum == tempNum)
				continue;

			u->tempNum = tempNum;
			qfq.enemyFakes->push_back(u);
		}
	}

	return;
}

} // namespace circuit
