/*
 * AllyTeam.h
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_ALLYTEAM_H_
#define SRC_CIRCUIT_SETUP_ALLYTEAM_H_

#include <vector>
#include <map>
#include <unordered_map>
#include <string.h>

namespace springai {
	class AIFloat3;
	class OOAICallback;
	class UnitDef;
	class Unit;
}

namespace circuit {

class CCircuitAI;
class CCircuitUnit;
class CCircuitDef;

class CAllyTeam {
public:
	union SBox {
		struct {
			float bottom;
			float left;
			float right;
			float top;
		};
		float edge[4];

		bool ContainsPoint(const springai::AIFloat3& point) const;
	};

public:
	CAllyTeam(const std::vector<int>& tids, const SBox& sb);
	virtual ~CAllyTeam();

	int GetSize() const;
	const SBox& GetStartBox() const;

	void UpdateUnits(int frame, springai::OOAICallback* callback);

private:
	std::vector<int> teamIds;
	SBox startBox;

	int lastUpdate;
	std::map<int, CCircuitUnit*> friendlyUnits;  // owner
	std::map<int, CCircuitUnit*> enemyUnits;  // owner

// ---- UnitDefs ---- BEGIN
private:
	struct cmp_str {
		bool operator()(char const* a, char const* b) {
			return strcmp(a, b) < 0;
		}
	};
public:
	using UnitDefs = std::map<const char*, springai::UnitDef*, cmp_str>;
	using IdUnitDefs = std::unordered_map<int, springai::UnitDef*>;
	using CircuitDefs = std::unordered_map<springai::UnitDef*, CCircuitDef*>;

	UnitDefs* GetDefsByName();
	IdUnitDefs* GetDefsById();
	CircuitDefs* GetCircuitDefs();
	void Init(CCircuitAI* circuit);
	void Release();
private:
	UnitDefs defsByName;  // owner
	IdUnitDefs defsById;
	std::unordered_map<springai::UnitDef*, CCircuitDef*> circuitDefs;  // owner

	int initCount;
// ---- UnitDefs ---- END
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_ALLYTEAM_H_
