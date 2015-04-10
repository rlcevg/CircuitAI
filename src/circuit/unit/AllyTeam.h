/*
 * AllyTeam.h
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_ALLYTEAM_H_
#define SRC_CIRCUIT_SETUP_ALLYTEAM_H_

#include "unit/CircuitUnit.h"

#include <vector>
#include <map>

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

	void Init();
	void Release();
	void UpdateUnits(CCircuitAI* circuit);

private:
	std::vector<Id> teamIds;
	SBox startBox;

	int initCount;
	int lastUpdate;
	std::map<CCircuitUnit::Id, CCircuitUnit*> friendlyUnits;  // owner
	std::map<CCircuitUnit::Id, CCircuitUnit*> enemyUnits;  // owner
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_ALLYTEAM_H_
