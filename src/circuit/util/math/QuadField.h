/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef QUAD_FIELD_H
#define QUAD_FIELD_H

#include <algorithm>
#include <array>
#include <vector>

#include "AIFloat3.h"
#include "System/type2.h"

namespace circuit {

using namespace springai;

class CAllyUnit;
class CEnemyUnit;
struct QuadFieldQuery;

template<typename T>
class QueryVectorCache {
public:
	typedef std::pair<bool, std::vector<T>> PairType;

	std::vector<T>* ReserveVector(size_t base = 0, size_t capa = 1024) {
		const auto pred = [](const PairType& p) { return (!p.first); };
		const auto iter = std::find_if(vectors.begin() + base, vectors.end(), pred);

		if (iter != vectors.end()) {
			iter->first = true;
			iter->second.clear();
			iter->second.reserve(capa);
			return &iter->second;
		}

		assert(false);
		return nullptr;
	}

	void ReserveAll(size_t capa) {
		for (size_t i = 0; i < vectors.size(); ++i) {
			ReserveVector(i, capa);
		}
	}

	void ReleaseVector(const std::vector<T>* released) {
		if (released == nullptr)
			return;

		const auto pred = [&](const PairType& p) { return (&p.second == released); };
		const auto iter = std::find_if(vectors.begin(), vectors.end(), pred);

		if (iter == vectors.end()) {
			assert(false);
			return;
		}

		iter->first = false;
	}
	void ReleaseAll() {
		for (auto& pair: vectors) {
			ReleaseVector(&pair.second);
		}
	}
private:
	// There should at most be 2 concurrent users of each vector type
	// using 3 to be safe, increase this number if the assertions below
	// fail
	std::array<PairType, 3> vectors = {{{false, {}}, {false, {}}, {false, {}}}};
};

class CQuadField
{
private:
	CQuadField(const CQuadField&);
	const CQuadField& operator=(const CQuadField&);

public:
	CQuadField() : numQuadsX(0), numQuadsZ(0), quadSizeX(0), quadSizeZ(0) {}
	~CQuadField() {}

	/*
	needed to support dynamic resizing (not used yet)
	in large games the average loading factor (number of objects per quad)
	can grow too large to maintain amortized constant performance so more
	quads are needed

	static void Resize(int quadSize);
	*/

	void Init(int2 mapDims, int quadSize);
	void Kill();

	void GetQuads(QuadFieldQuery& qfq, AIFloat3 pos, float radius);
	void GetQuadsRectangle(QuadFieldQuery& qfq, const AIFloat3& mins, const AIFloat3& maxs);
	void GetQuadsOnRay(QuadFieldQuery& qfq, const AIFloat3& start, const AIFloat3& dir, float length);

	/**
	 * Returns all units within @c radius of @c pos,
	 * and treats each unit as a 3D point object
	 */
	void GetAllyUnits(QuadFieldQuery& qfq, const AIFloat3& pos, float radius);
	/**
	 * Returns all units within @c radius of @c pos,
	 * takes the 3D model radius of each unit into account,
 	 * and performs the search within a sphere or cylinder depending on @c spherical
	 */
	void GetAllyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& pos, float radius, bool spherical = true);
	/**
	 * Returns all units within the rectangle defined by
	 * mins and maxs, which extends infinitely along the y-axis
	 */
	void GetAllyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& mins, const AIFloat3& maxs);

	/**
	 * Returns all units within @c radius of @c pos,
	 * and treats each unit as a 3D point object
	 */
	void GetEnemyUnits(QuadFieldQuery& qfq, const AIFloat3& pos, float radius);
	/**
	 * Returns all units within @c radius of @c pos,
	 * takes the 3D model radius of each unit into account,
 	 * and performs the search within a sphere or cylinder depending on @c spherical
	 */
	void GetEnemyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& pos, float radius, bool spherical = true);
	/**
	 * Returns all units within the rectangle defined by
	 * mins and maxs, which extends infinitely along the y-axis
	 */
	void GetEnemyUnitsExact(QuadFieldQuery& qfq, const AIFloat3& mins, const AIFloat3& maxs);

	bool InsertAllyUnitIf(CAllyUnit* unit, const AIFloat3& upos, const AIFloat3& wpos);
	bool RemoveAllyUnitIf(CAllyUnit* unit, const AIFloat3& upos, const AIFloat3& wpos);

	void MovedAllyUnit(CAllyUnit* unit, const AIFloat3& upos);
	void RemoveAllyUnit(CAllyUnit* unit, const AIFloat3& upos);

	bool InsertEnemyUnitIf(CEnemyUnit* unit, const AIFloat3& wpos);
	bool RemoveEnemyUnitIf(CEnemyUnit* unit, const AIFloat3& wpos);

	void MovedEnemyUnit(CEnemyUnit* unit);
	void RemoveEnemyUnit(CEnemyUnit* unit);

	void ReleaseVector(std::vector<CAllyUnit*>* v ) { tempAllyUnits.ReleaseVector(v); }
	void ReleaseVector(std::vector<CEnemyUnit*>* v) { tempEnemyUnits.ReleaseVector(v); }
	void ReleaseVector(std::vector<int>* v        ) { tempQuads.ReleaseVector(v); }

	struct Quad {
	public:
		Quad() = default;
		Quad(const Quad& q) = delete;
		Quad(Quad&& q) { *this = std::move(q); }

		Quad& operator = (const Quad& q) = delete;
		Quad& operator = (Quad&& q) {
			allyUnits = std::move(q.allyUnits);
			enemyUnits = std::move(q.enemyUnits);
			return *this;
		}

		void Clear() {
			allyUnits.clear();
			enemyUnits.clear();
		}

	public:
		std::vector<CAllyUnit*> allyUnits;
		std::vector<CEnemyUnit*> enemyUnits;
	};

	const Quad& GetQuad(unsigned i) const {
		assert(i < baseQuads.size());
		return baseQuads[i];
	}
	const Quad& GetQuadAt(unsigned x, unsigned z) const {
		assert(unsigned(numQuadsX * z + x) < baseQuads.size());
		return baseQuads[numQuadsX * z + x];
	}

	int GetNumQuadsX() const { return numQuadsX; }
	int GetNumQuadsZ() const { return numQuadsZ; }

	int GetQuadSizeX() const { return quadSizeX; }
	int GetQuadSizeZ() const { return quadSizeZ; }

	constexpr static unsigned int BASE_QUAD_SIZE = 128;

private:
	int2 WorldPosToQuadField(const AIFloat3 p) const;
	int WorldPosToQuadFieldIdx(const AIFloat3 p) const;

private:
	std::vector<Quad> baseQuads;

	// preallocated vectors for Get*Exact functions
	QueryVectorCache<CAllyUnit*> tempAllyUnits;
	QueryVectorCache<CEnemyUnit*> tempEnemyUnits;
	QueryVectorCache<int> tempQuads;

	float2 invQuadSize;

	int numQuadsX;
	int numQuadsZ;

	int quadSizeX;
	int quadSizeZ;

private:
	int GetTempNum() { return tempNum++; }
	/**
	* @brief temp num
	*
	* Used for getting temporary but unique numbers
	* (increase after each use)
	*/
	int tempNum = 1;
};

struct QuadFieldQuery {
	QuadFieldQuery(CQuadField& qf) : quadField(qf) {}
	~QuadFieldQuery() {
		quadField.ReleaseVector(allyUnits);
		quadField.ReleaseVector(enemyUnits);
		quadField.ReleaseVector(quads);
	}

	CQuadField& quadField;
	std::vector<CAllyUnit*>* allyUnits = nullptr;
	std::vector<CEnemyUnit*>* enemyUnits = nullptr;
	std::vector<int>* quads = nullptr;
};

} // namespace circuit

#endif /* QUAD_FIELD_H */
