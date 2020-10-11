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
#include "unit/enemy/EnemyUnit.h"
#include "util/MaskHandler.h"

namespace circuit {

class CCircuitAI;
class CQuadField;
struct STerrainMapArea;

class CEnemyManager {
public:
	using EnemyUnits = std::unordered_map<ICoreUnit::Id, CEnemyUnit*>;
	using EnemyFakes = std::set<CEnemyFake*>;
	struct SEnemyGroup {
		SEnemyGroup(const springai::AIFloat3& p) : pos(p), cost(0.f), threat(0.f) {}
		std::vector<ICoreUnit::Id> units;
		springai::AIFloat3 pos;
		std::array<float, CMaskHandler::GetMaxMasks()> roleCosts{{0.f}};
		float cost;
		float threat;  // thr_mod applied
	};

	CEnemyManager(CCircuitAI* circuit);
	virtual ~CEnemyManager();

	void SetAuthority(CCircuitAI* authority) { circuit = authority; }

	CEnemyUnit* GetEnemyUnit(ICoreUnit::Id unitId) const;

	const std::set<CEnemyUnit*>& GetDyingEnemies() const { return enemyDying; }

	const std::vector<SEnemyData>& GetHostileDatas() const { return hostileDatas; }
	const std::vector<SEnemyData>& GetPeaceDatas() const { return peaceDatas; }

	void UpdateEnemyDatas(CQuadField& quadField);

	void PrepareUpdate();
	void EnqueueUpdate();
	bool IsUpdating() const { return isUpdating; }

	bool UnitInLOS(CEnemyUnit* data);
	std::pair<CEnemyUnit*, bool> RegisterEnemyUnit(ICoreUnit::Id unitId, bool isInLOS);
	CEnemyUnit* RegisterEnemyUnit(springai::Unit* e);

	CEnemyFake* RegisterEnemyFake(CCircuitDef* cdef, const springai::AIFloat3& pos, int timeout);
	void UnregisterEnemyFake(CEnemyFake* data);

	void UnregisterEnemyUnit(CEnemyUnit* data);
	void DyingEnemy(CEnemyUnit* enemy, int frame);
private:
	void DyingEnemy(CEnemyUnit* enemy);
	void DeleteEnemyUnit(CEnemyUnit* data);

public:
	float GetEnemyCost(CCircuitDef::RoleT type) const {
		return enemyInfos[type].cost;
	}
	float GetEnemyThreat(CCircuitDef::RoleT type) const {
		return enemyInfos[type].threat;
	}
	void AddEnemyCost(const CEnemyUnit* e);
	void DelEnemyCost(const CEnemyUnit* e);
	float GetMobileThreat() const { return mobileThreat; }
	float GetStaticThreat() const { return staticThreat; }
	float GetEnemyThreat() const { return mobileThreat + staticThreat; }
	bool IsAirValid() const { return GetEnemyThreat(ROLE_TYPE(AA)) <= maxAAThreat; }
	bool IsEnemyNear(const springai::AIFloat3& pos, float maxThreat);

	const std::vector<SEnemyGroup>& GetEnemyGroups() const { return enemyGroups; }
	const springai::AIFloat3& GetEnemyPos() const { return enemyPos; }
	float GetMaxGroupThreat() const { return enemyGroups[maxThreatGroupIdx].threat; }
	float GetEnemyMobileCost() const { return enemyMobileCost; }

	void UpdateAreaUsers(CCircuitAI* ai);
	void SetAreaUpdated(bool value) { isAreaUpdated = value; }
	const std::unordered_set<const STerrainMapArea*>& GetEnemyAreas() const { return enemyAreas; }

private:
	void ReadConfig();
	void KMeansIteration();

	struct SGroupData {
		std::vector<SEnemyGroup> enemyGroups;
		springai::AIFloat3 enemyPos;
		int maxThreatGroupIdx;
	};

	void Prepare(SGroupData& groupData);
	void Update();
	void Apply();
	void SwapBuffers();
	SGroupData* GetNextGroupData() {
		return (pGroupData.load() == &groupData0) ? &groupData1 : &groupData0;
	}

	CCircuitAI* circuit;

	EnemyUnits enemyUnits;  // owner
	EnemyFakes enemyFakes;  // owner

	std::vector<CEnemyUnit*> enemyUpdates;
	unsigned int enemyIterator;

	int dyingFrame;
	std::set<CEnemyUnit*> enemyDying;

	std::vector<SEnemyData> hostileDatas;  // immutable during threaded processing
	std::vector<SEnemyData> peaceDatas;  // immutable during threaded processing

	SGroupData groupData0, groupData1;  // Double-buffer for threading
	std::atomic<SGroupData*> pGroupData;
	std::vector<SEnemyGroup>& enemyGroups;
	springai::AIFloat3 enemyPos;
	int maxThreatGroupIdx;
	bool isUpdating;

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
	std::array<SEnemyInfo, CMaskHandler::GetMaxMasks()> enemyInfos{{{0.f}, {0.f}}};

	bool isAreaUpdated;
	std::unordered_set<const STerrainMapArea*> enemyAreas;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_ENEMY_ENEMYMANAGER_H_
