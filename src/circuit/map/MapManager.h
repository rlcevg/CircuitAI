/*
 * MapManager.h
 *
 *  Created on: Dec 21, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MAP_MAPMANAGER_H_
#define SRC_CIRCUIT_MAP_MAPMANAGER_H_

#include "unit/enemy/EnemyManager.h"

namespace circuit {

class CCircuitAI;
class CThreatMap;
class CInfluenceMap;

class CMapManager {
public:
	CMapManager(CCircuitAI* circuit, float decloakRadius);
	virtual ~CMapManager();

	void SetAuthority(CCircuitAI* authority) { circuit = authority; }
	CCircuitAI* GetCircuit() const { return circuit; }

	CThreatMap* GetThreatMap() const { return threatMap; }
	CInfluenceMap* GetInflMap() const { return inflMap; }

	const CEnemyManager::EnemyUnits& GetHostileUnits() const { return hostileUnits; }
	const CEnemyManager::EnemyUnits& GetPeaceUnits() const { return peaceUnits; }
	const CEnemyManager::EnemyFakes& GetEnemyFakes() const { return enemyFakes; }

	void PrepareUpdate();
	void EnqueueUpdate();
	bool IsUpdating() const;
	bool HostileInLOS(CEnemyUnit* enemy);
	bool PeaceInLOS(CEnemyUnit* enemy);

	bool IsSuddenThreat(CEnemyUnit* enemy) const;

	bool EnemyEnterLOS(CEnemyUnit* enemy);
	void EnemyLeaveLOS(CEnemyUnit* enemy);
	void EnemyEnterRadar(CEnemyUnit* enemy);
	void EnemyLeaveRadar(CEnemyUnit* enemy);
	bool EnemyDestroyed(CEnemyUnit* enemy);

	void AddFakeEnemy(CEnemyUnit* enemy);
	void DelFakeEnemy(CEnemyUnit* enemy);

	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

private:
	CCircuitAI* circuit;

	CThreatMap* threatMap;
	CInfluenceMap* inflMap;

	CEnemyManager::EnemyUnits hostileUnits;
	CEnemyManager::EnemyUnits peaceUnits;
	CEnemyManager::EnemyFakes enemyFakes;

//	IntVec radarMap;
	IntVec sonarMap;
	IntVec losMap;

	int radarWidth;
	int radarResConv;
	int losWidth;
	int losResConv;
};

} // namespace circuit

#endif // SRC_CIRCUIT_MAP_MAPMANAGER_H_
