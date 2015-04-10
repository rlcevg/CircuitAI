/*
 * AllyTeam.h
 *
 *  Created on: Apr 7, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUP_ALLYTEAM_H_
#define SRC_CIRCUIT_SETUP_ALLYTEAM_H_

#include "unit/CircuitUnit.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CCircuitAI;
class CTerrainManager;

class CAllyTeam {
public:
	using Id = int;
	using Units = std::unordered_map<CCircuitUnit::Id, CCircuitUnit*>;
	using TeamIds = std::unordered_set<Id>;
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
	CAllyTeam(const TeamIds& tids, const SBox& sb);
	virtual ~CAllyTeam();

	int GetSize() const;
	const TeamIds& GetTeamIds() const;
	const SBox& GetStartBox() const;

	void Init(CCircuitAI* circuit);
	void Release();

	void UpdateFriendlyUnits(CCircuitAI* circuit);
	CCircuitUnit* GetFriendlyUnit(CCircuitUnit::Id unitId);
	const Units& GetFriendlyUnits() const;

	void AddEnemyUnit(CCircuitUnit* unit);
	void RemoveEnemyUnit(CCircuitUnit* unit);
	CCircuitUnit* GetEnemyUnit(CCircuitUnit::Id unitId);
	const Units& GetEnemyUnits() const;

	std::shared_ptr<CTerrainManager>& GetTerrainManager();

private:
	TeamIds teamIds;
	SBox startBox;

	int initCount;
	int lastUpdate;
	Units friendlyUnits;  // owner
	Units enemyUnits;  // owner

	std::shared_ptr<CTerrainManager> terrainManager;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUP_ALLYTEAM_H_
