/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_CIRCUIT_H_
#define SRC_CIRCUIT_CIRCUIT_H_

#include "unit/AllyTeam.h"
#include "unit/CircuitDef.h"
#include "util/Defines.h"

#include <memory>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>
#include <deque>

struct SSkirmishAICallback;

namespace circuit {

#define ERROR_UNKNOWN			200
#define ERROR_INIT				(ERROR_UNKNOWN + EVENT_INIT)
#define ERROR_RELEASE			(ERROR_UNKNOWN + EVENT_RELEASE)
#define ERROR_UPDATE			(ERROR_UNKNOWN + EVENT_UPDATE)
#define ERROR_UNIT_CREATED		(ERROR_UNKNOWN + EVENT_UNIT_CREATED)
#define ERROR_UNIT_FINISHED		(ERROR_UNKNOWN + EVENT_UNIT_FINISHED)
#define ERROR_UNIT_IDLE			(ERROR_UNKNOWN + EVENT_UNIT_IDLE)
#define ERROR_UNIT_MOVE_FAILED	(ERROR_UNKNOWN + EVENT_UNIT_MOVE_FAILED)
#define ERROR_UNIT_DAMAGED		(ERROR_UNKNOWN + EVENT_UNIT_DAMAGED)
#define ERROR_UNIT_DESTROYED	(ERROR_UNKNOWN + EVENT_UNIT_DESTROYED)
#define ERROR_UNIT_GIVEN		(ERROR_UNKNOWN + EVENT_UNIT_GIVEN)
#define ERROR_UNIT_CAPTURED		(ERROR_UNKNOWN + EVENT_UNIT_CAPTURED)
#define ERROR_ENEMY_ENTER_LOS	(ERROR_UNKNOWN + EVENT_ENEMY_ENTER_LOS)
#define ERROR_ENEMY_LEAVE_LOS	(ERROR_UNKNOWN + EVENT_ENEMY_LEAVE_LOS)
#define ERROR_ENEMY_ENTER_RADAR	(ERROR_UNKNOWN + EVENT_ENEMY_ENTER_RADAR)
#define ERROR_ENEMY_LEAVE_RADAR	(ERROR_UNKNOWN + EVENT_ENEMY_LEAVE_RADAR)
#define ERROR_ENEMY_DAMAGED		(ERROR_UNKNOWN + EVENT_ENEMY_DAMAGED)
#define ERROR_ENEMY_DESTROYED	(ERROR_UNKNOWN + EVENT_ENEMY_DESTROYED)
#define ERROR_LOAD				(ERROR_UNKNOWN + EVENT_LOAD)
#define ERROR_SAVE				(ERROR_UNKNOWN + EVENT_SAVE)
#define ERROR_ENEMY_CREATED		(ERROR_UNKNOWN + EVENT_ENEMY_CREATED)
#define LOG(fmt, ...)	GetLog()->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

class CGameAttribute;
class CSetupManager;
class CThreatMap;
class CPathFinder;
class CTerrainManager;
class CBuilderManager;
class CFactoryManager;
class CEconomyManager;
class CMilitaryManager;
class CScheduler;
class IModule;
class CCircuitUnit;
class CEnemyUnit;
#ifdef DEBUG_VIS
class CDebugDrawer;
#endif

constexpr char version[]{"1.0.5"};

class CException: public std::exception {
public:
	CException(const char* r) : std::exception(), reason(r) {}
	virtual const char* what() const throw() {
		return reason;
	}
	const char* reason;
};

class CCircuitAI {
public:
	CCircuitAI(springai::OOAICallback* callback);
	virtual ~CCircuitAI();

// ---- AI Event handler ---- BEGIN
public:
	int HandleEvent(int topic, const void* data);
	void NotifyGameEnd();
	void NotifyResign();
//	void NotifyShutdown();
	void Resign(int newTeamId);
private:
	typedef int (CCircuitAI::*EventHandlerPtr)(int topic, const void* data);
	int HandleGameEvent(int topic, const void* data);
	int HandleEndEvent(int topic, const void* data);
	int HandleResignEvent(int topic, const void* data);
//	int HandleShutdownEvent(int topic, const void* data);
	EventHandlerPtr eventHandler;

	int ownerTeamId;
	springai::Economy* economy;
	springai::Resource* metalRes;
	springai::Resource* energyRes;

//	bool IsCorrupted() const { return !corrupts.empty(); }
//	std::deque<std::string> corrupts;
// ---- AI Event handler ---- END

private:
//	bool IsModValid();
	void CheatPreload();
	int Init(int skirmishAIId, const struct SSkirmishAICallback* sAICallback);
	int Release(int reason);
	int Update(int frame);
	int Message(int playerId, const char* message);
	int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	int UnitFinished(CCircuitUnit* unit);
	int UnitIdle(CCircuitUnit* unit);
	int UnitMoveFailed(CCircuitUnit* unit);
	int UnitDamaged(CCircuitUnit* unit, CEnemyUnit* attacker/*, int weaponId*/);
	int UnitDestroyed(CCircuitUnit* unit, CEnemyUnit* attacker);
	int UnitGiven(ICoreUnit::Id unitId, int oldTeamId, int newTeamId);
	int UnitCaptured(ICoreUnit::Id unitId, int oldTeamId, int newTeamId);
	int EnemyEnterLOS(CEnemyUnit* enemy);
	int EnemyLeaveLOS(CEnemyUnit* enemy);
	int EnemyEnterRadar(CEnemyUnit* enemy);
	int EnemyLeaveRadar(CEnemyUnit* enemy);
	int EnemyDamaged(CEnemyUnit* enemy);
	int EnemyDestroyed(CEnemyUnit* enemy);
	int PlayerCommand(std::vector<CCircuitUnit*>& units);
//	int CommandFinished(CCircuitUnit* unit, int commandTopicId, springai::Command* cmd);
	int Load(std::istream& is);
	int Save(std::ostream& os);
	int LuaMessage(const char* inData);

// ---- Units ---- BEGIN
public:
	using Units = std::map<ICoreUnit::Id, CCircuitUnit*>;
private:
	CCircuitUnit* GetOrRegTeamUnit(ICoreUnit::Id unitId);
	CCircuitUnit* RegisterTeamUnit(ICoreUnit::Id unitId);
	void UnregisterTeamUnit(CCircuitUnit* unit);
	void DeleteTeamUnit(CCircuitUnit* unit);
public:
	void Garbage(CCircuitUnit* unit, const char* reason);
	CCircuitUnit* GetTeamUnit(ICoreUnit::Id unitId) const;
	const Units& GetTeamUnits() const { return teamUnits; }

	void UpdateFriendlyUnits() { allyTeam->UpdateFriendlyUnits(this); }
	CAllyUnit* GetFriendlyUnit(springai::Unit* u) const;
	CAllyUnit* GetFriendlyUnit(ICoreUnit::Id unitId) const { return allyTeam->GetFriendlyUnit(unitId); }
	const CAllyTeam::Units& GetFriendlyUnits() const { return allyTeam->GetFriendlyUnits(); }

	using EnemyUnits = std::map<ICoreUnit::Id, CEnemyUnit*>;
private:
	std::pair<CEnemyUnit*, bool> RegisterEnemyUnit(ICoreUnit::Id unitId, bool isInLOS = false);
	CEnemyUnit* RegisterEnemyUnit(springai::Unit* e);
	void UnregisterEnemyUnit(CEnemyUnit* unit);
	void UpdateEnemyUnits();
public:
	CEnemyUnit* GetEnemyUnit(springai::Unit* u) const { return GetEnemyUnit(u->GetUnitId()); }
	CEnemyUnit* GetEnemyUnit(ICoreUnit::Id unitId) const;
	const EnemyUnits& GetEnemyUnits() const { return enemyUnits; }

	CAllyTeam* GetAllyTeam() const { return allyTeam; }

	void DisableControl(const std::string data);
	void EnableControl(const std::string data);

	void AddActionUnit(CCircuitUnit* unit) { actionUnits.push_back(unit); }

private:
	void ActionUpdate();

	Units teamUnits;  // owner
	EnemyUnits enemyUnits;  // owner
	CAllyTeam* allyTeam;
	int uEnemyMark;
	int kEnemyMark;

	std::vector<CCircuitUnit*> actionUnits;
	unsigned int actionIterator;

	std::set<CCircuitUnit*> garbage;
// ---- Units ---- END

// ---- AIOptions.lua ---- BEGIN
public:
	bool IsCheating() const { return isCheating; }
	bool IsAllyAware() const { return isAllyAware; }
	bool IsCommMerge() const { return isCommMerge; }
private:
	std::string InitOptions();
	bool isCheating;
	bool isAllyAware;
	bool isCommMerge;
// ---- AIOptions.lua ---- END

// ---- UnitDefs ---- BEGIN
public:
	using CircuitDefs = std::unordered_map<CCircuitDef::Id, CCircuitDef*>;
	using NamedDefs = std::map<const char*, CCircuitDef*, cmp_str>;

	const CircuitDefs& GetCircuitDefs() const { return defsById; }
	CCircuitDef* GetCircuitDef(const char* name);
	CCircuitDef* GetCircuitDef(CCircuitDef::Id unitDefId);
//	const std::vector<CCircuitDef*>& GetKnownDefs() const { return knownDefs; }
private:
	void InitUnitDefs(float& outDcr);
//	void InitKnownDefs(const CCircuitDef* commDef);
	CircuitDefs defsById;  // owner
	NamedDefs defsByName;
//	std::vector<CCircuitDef*> knownDefs;
// ---- UnitDefs ---- END

public:
	bool IsInitialized() const { return isInitialized; }
	CGameAttribute* GetGameAttribute() const { return gameAttribute.get(); }
	std::shared_ptr<CScheduler>& GetScheduler() { return scheduler; }
	int GetLastFrame()    const { return lastFrame; }
	int GetSkirmishAIId() const { return skirmishAIId; }
	int GetTeamId()       const { return teamId; }
	int GetAllyTeamId()   const { return allyTeamId; }
	springai::OOAICallback* GetCallback()   const { return callback; }
	springai::Log*          GetLog()        const { return log.get(); }
	springai::Game*         GetGame()       const { return game.get(); }
	springai::Map*          GetMap()        const { return map.get(); }
	springai::Lua*          GetLua()        const { return lua.get(); }
	springai::Pathing*      GetPathing()    const { return pathing.get(); }
	springai::Drawer*       GetDrawer()     const { return drawer.get(); }
	springai::SkirmishAI*   GetSkirmishAI() const { return skirmishAI.get(); }
	springai::Team*         GetTeam()       const { return team.get(); }
	CSetupManager*    GetSetupManager()    const { return setupManager.get(); }
	CMetalManager*    GetMetalManager()    const { return metalManager.get(); }
	CThreatMap*       GetThreatMap()       const { return threatMap.get(); }
	CPathFinder*      GetPathfinder()      const { return pathfinder.get(); }
	CTerrainManager*  GetTerrainManager()  const { return terrainManager.get(); }
	CBuilderManager*  GetBuilderManager()  const { return builderManager.get(); }
	CFactoryManager*  GetFactoryManager()  const { return factoryManager.get(); }
	CEconomyManager*  GetEconomyManager()  const { return economyManager.get(); }
	CMilitaryManager* GetMilitaryManager() const { return militaryManager.get(); }

	int GetAirCategory()   const { return airCategory; }
	int GetLandCategory()  const { return landCategory; }
	int GetWaterCategory() const { return waterCategory; }
	int GetBadCategory()   const { return badCategory; }
	int GetGoodCategory()  const { return goodCategory; }

private:
	// debug
//	void DrawClusters();

	bool isInitialized;
	bool isResigned;
	int lastFrame;
	int skirmishAIId;
	int teamId;
	int allyTeamId;
	const struct SSkirmishAICallback* sAICallback;
	springai::OOAICallback*               callback;
	std::unique_ptr<springai::Log>        log;
	std::unique_ptr<springai::Game>       game;
	std::unique_ptr<springai::Map>        map;
	std::unique_ptr<springai::Lua>        lua;
	std::unique_ptr<springai::Pathing>    pathing;
	std::unique_ptr<springai::Drawer>     drawer;
	std::unique_ptr<springai::SkirmishAI> skirmishAI;
	std::unique_ptr<springai::Team>       team;

	static std::unique_ptr<CGameAttribute> gameAttribute;
	static unsigned int gaCounter;
	void CreateGameAttribute(unsigned int seed);
	void DestroyGameAttribute();
	std::shared_ptr<CScheduler> scheduler;
	std::shared_ptr<CSetupManager> setupManager;
	std::shared_ptr<CMetalManager> metalManager;
	std::shared_ptr<CThreatMap> threatMap;
	std::shared_ptr<CPathFinder> pathfinder;
	std::shared_ptr<CTerrainManager> terrainManager;
	std::shared_ptr<CBuilderManager> builderManager;
	std::shared_ptr<CFactoryManager> factoryManager;
	std::shared_ptr<CEconomyManager> economyManager;
	std::shared_ptr<CMilitaryManager> militaryManager;
	std::vector<std::shared_ptr<IModule>> modules;

	// TODO: Move into GameAttribute? Or use locally
	int airCategory;  // over surface
	int landCategory;  // on surface
	int waterCategory;  // under surface
	int badCategory;
	int goodCategory;

#ifdef DEBUG_VIS
private:
	std::shared_ptr<CDebugDrawer> debugDrawer;
public:
	std::shared_ptr<CDebugDrawer>& GetDebugDrawer() { return debugDrawer; }
#endif
};

} // namespace circuit

#endif // SRC_CIRCUIT_CIRCUIT_H_
