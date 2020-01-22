/*
 * EnemyManager.h
 *
 *  Created on: Dec 25, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ENEMY_ENEMYMANAGER_H_
#define SRC_CIRCUIT_UNIT_ENEMY_ENEMYMANAGER_H_

#include "unit/CoreUnit.h"
#include "unit/CircuitDef.h"

namespace circuit {

class CCircuitAI;
class CEnemyUnit;

class CEnemyManager {
public:
	using EnemyUnits = std::map<ICoreUnit::Id, CEnemyUnit*>;
	struct SEnemyGroup {
		SEnemyGroup(const springai::AIFloat3& p) : pos(p), cost(0.f), threat(0.f) {}
		std::vector<ICoreUnit::Id> units;
		springai::AIFloat3 pos;
		std::array<float, static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_)> roleCosts{{0.f}};
		float cost;
		float threat;  // thr_mod applied
	};

	CEnemyManager(CCircuitAI* circuit);
	virtual ~CEnemyManager();

	void SetAuthority(CCircuitAI* authority) { circuit = authority; }

	CEnemyUnit* GetEnemyUnit(ICoreUnit::Id unitId) const;

	const std::vector<ICoreUnit::Id>& GetGarbage() const { return enemyGarbage; }

	void UpdateEnemyDatas();

	bool UnitInLOS(CEnemyUnit* data);
	std::pair<CEnemyUnit*, bool> RegisterEnemyUnit(ICoreUnit::Id unitId, bool isInLOS);
	CEnemyUnit* RegisterEnemyUnit(springai::Unit* e);

	void UnregisterEnemyUnit(CEnemyUnit* data);
private:
	void DeleteEnemyUnit(CEnemyUnit* data);
	void GarbageEnemy(CEnemyUnit* enemy);

public:
	float GetEnemyCost(CCircuitDef::RoleType type) const {
		return enemyInfos[static_cast<CCircuitDef::RoleT>(type)].cost;
	}
	float GetEnemyThreat(CCircuitDef::RoleType type) const {
		return enemyInfos[static_cast<CCircuitDef::RoleT>(type)].threat;
	}
	void AddEnemyCost(const CEnemyUnit* e);
	void DelEnemyCost(const CEnemyUnit* e);
	float GetMobileThreat() const { return mobileThreat; }
	float GetStaticThreat() const { return staticThreat; }
	float GetEnemyThreat() const { return mobileThreat + staticThreat; }
	bool IsAirValid() const { return GetEnemyThreat(CCircuitDef::RoleType::AA) <= maxAAThreat; }

	const std::vector<SEnemyGroup>& GetEnemyGroups() const { return enemyGroups; }
	const springai::AIFloat3& GetEnemyPos() const { return enemyPos; }
	void UpdateEnemyGroups() { KMeansIteration(); }

	float GetEnemyMobileCost() const { return enemyMobileCost; }

private:
	void ReadConfig();
	void KMeansIteration();

	CCircuitAI* circuit;

	EnemyUnits enemyUnits;  // owner

	std::vector<CEnemyUnit*> enemyUpdates;
	unsigned int enemyIterator;

	std::vector<ICoreUnit::Id> enemyGarbage;

	std::vector<SEnemyGroup> enemyGroups;
	springai::AIFloat3 enemyPos;

	float enemyMobileCost;
	float mobileThreat;  // thr_mod.mobile applied
	float staticThreat;  // thr_mod.static applied
	struct SInitThreatMod {
		float inMobile;
		float inStatic;
	} initThrMod;
	float maxAAThreat;
	struct SEnemyInfo {
		float cost;
		float threat;
	};
	std::array<SEnemyInfo, static_cast<CCircuitDef::RoleT>(CCircuitDef::RoleType::_SIZE_)> enemyInfos{{{0.f}, {0.f}}};
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMY_ENEMYMANAGER_H_
