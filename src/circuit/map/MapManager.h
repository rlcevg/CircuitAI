/*
 * MapManager.h
 *
 *  Created on: Dec 21, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_MAP_MAPMANAGER_H_
#define SRC_CIRCUIT_MAP_MAPMANAGER_H_

#include "unit/EnemyUnit.h"
#include "CircuitAI.h"

class CThreatMap;
class CInfluenceMap;

namespace circuit {

class CMapManager {
public:
	CMapManager(CCircuitAI* circuit, float decloakRadius);
	virtual ~CMapManager();

	void SetAuthority(CCircuitAI* authority) { circuit = authority; }
	CCircuitAI* GetCircuit() const { return circuit; }

	CThreatMap* GetThreatMap() const { return threatMap; }
	CInfluenceMap* GetInflMap() const { return inflMap; }

	void EnqueueUpdate();

	const std::vector<CEnemyUnit::SEnemyData>& GetHostileDatas() const { return hostileDatas; }
	const std::vector<CEnemyUnit::SEnemyData>& GetPeaceDatas() const { return peaceDatas; }

	bool EnemyEnterLOS(CEnemyUnit* enemy);
	void EnemyLeaveLOS(CEnemyUnit* enemy);
	void EnemyEnterRadar(CEnemyUnit* enemy);
	void EnemyLeaveRadar(CEnemyUnit* enemy);
	bool EnemyDestroyed(CEnemyUnit* enemy);

private:
	bool IsInLOS(const springai::AIFloat3& pos) const;
//	bool IsInRadar(const springai::AIFloat3& pos) const;

	CCircuitAI* circuit;

	CThreatMap* threatMap;
	CInfluenceMap* inflMap;

	CCircuitAI::EnemyUnits hostileUnits;
	CCircuitAI::EnemyUnits peaceUnits;

	std::vector<CEnemyUnit::SEnemyData> hostileDatas;
	std::vector<CEnemyUnit::SEnemyData> peaceDatas;

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
