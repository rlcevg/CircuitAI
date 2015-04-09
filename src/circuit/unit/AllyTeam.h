/*
 * AllyTeam.h
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_ALLYTEAM_H_
#define SRC_CIRCUIT_SETUP_ALLYTEAM_H_

#include "unit/CircuitUnit.h"
#include "unit/CircuitDef.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <string.h>

namespace springai {
	class AIFloat3;
	class OOAICallback;
}

namespace circuit {

class CCircuitAI;

class CAllyTeam {
public:
	using Id = int;
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
	CAllyTeam(const std::vector<Id>& tids, const SBox& sb);
	virtual ~CAllyTeam();

	int GetSize() const;
	const SBox& GetStartBox() const;

	void UpdateUnits(int frame, springai::OOAICallback* callback);

private:
	std::vector<Id> teamIds;
	SBox startBox;

	int lastUpdate;
	std::map<CCircuitUnit::Id, CCircuitUnit*> friendlyUnits;  // owner
	std::map<CCircuitUnit::Id, CCircuitUnit*> enemyUnits;  // owner

// ---- UnitDefs ---- BEGIN
private:
	struct cmp_str {
		bool operator()(char const* a, char const* b) {
			return strcmp(a, b) < 0;
		}
	};
public:
	using CircuitDefs = std::unordered_map<CCircuitDef::Id, CCircuitDef*>;
	using NCircuitDefs = std::map<const char*, CCircuitDef*, cmp_str>;

	NCircuitDefs* GetDefsByName();
	CircuitDefs* GetDefsById();
	void Init(CCircuitAI* circuit);
	void Release();
private:
	NCircuitDefs defsByName;
	CircuitDefs defsById;  // owner

	int initCount;
// ---- UnitDefs ---- END
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_ALLYTEAM_H_
