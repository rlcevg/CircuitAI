/*
 * EnemyManager.h
 *
 *  Created on: Dec 25, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_ENEMYMANAGER_H_
#define SRC_CIRCUIT_UNIT_ENEMYMANAGER_H_

#include "unit/CoreUnit.h"

namespace circuit {

class CCircuitAI;
class CEnemyUnit;

class CEnemyManager {
public:
	using EnemyUnits = std::map<ICoreUnit::Id, CEnemyUnit*>;

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

	CCircuitAI* circuit;

	EnemyUnits enemyUnits;  // owner

	std::vector<CEnemyUnit*> enemyUpdates;
	unsigned int enemyIterator;

	std::vector<ICoreUnit::Id> enemyGarbage;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMYMANAGER_H_
