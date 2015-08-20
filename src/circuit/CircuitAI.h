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

#include <memory>
#include <unordered_map>
#include <map>
#include <list>
#include <vector>
#include <string.h>

namespace springai {
	class OOAICallback;
	class Log;
	class Game;
	class Map;
	class Pathing;
	class Drawer;
	class SkirmishAI;
}
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
#define ERROR_ENEMY_DESTROYED	(ERROR_UNKNOWN + EVENT_ENEMY_DESTROYED)
#define LOG(fmt, ...)	GetLog()->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

class CGameAttribute;
class CSetupManager;
class CTerrainManager;
class CBuilderManager;
class CFactoryManager;
class CEconomyManager;
class CMilitaryManager;
class CScheduler;
class IModule;

class CCircuitAI {
public:
	enum class Difficulty: char {EASY, NORMAL, HARD};

public:
	CCircuitAI(springai::OOAICallback* callback);
	virtual ~CCircuitAI();

// ---- AI Event handler ---- BEGIN
public:
	int HandleEvent(int topic, const void* data);
	void NotifyGameEnd();
private:
	typedef int (CCircuitAI::*EventHandlerPtr)(int topic, const void* data);
	int HandleGameEvent(int topic, const void* data);
	int HandleEndEvent(int topic, const void* data);
	EventHandlerPtr eventHandler;
// ---- AI Event handler ---- END

private:
	bool IsModValid();
	int Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	int Release(int reason);
	int Update(int frame);
	int Message(int playerId, const char* message);
	int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	int UnitFinished(CCircuitUnit* unit);
	int UnitIdle(CCircuitUnit* unit);
	int UnitMoveFailed(CCircuitUnit* unit);
	int UnitDamaged(CCircuitUnit* unit, CCircuitUnit* attacker);
	int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);
	int UnitGiven(CCircuitUnit* unit, int oldTeamId, int newTeamId);  // TODO: Use Team class?
	int UnitCaptured(CCircuitUnit* unit, int oldTeamId, int newTeamId);  // TODO: Use Team class?
	int EnemyEnterLOS(CCircuitUnit* unit);
	int PlayerCommand(std::vector<CCircuitUnit*>& units);
//	int CommandFinished(CCircuitUnit* unit, int commandTopicId);
	int LuaMessage(const char* inData);

// ---- Units ---- BEGIN
private:
	CCircuitUnit* RegisterTeamUnit(CCircuitUnit::Id unitId);
	void UnregisterTeamUnit(CCircuitUnit* unit);
public:
	CCircuitUnit* GetTeamUnit(CCircuitUnit::Id unitId);
	const CAllyTeam::Units& GetTeamUnits() const { return teamUnits; }

	void UpdateFriendlyUnits() { allyTeam->UpdateFriendlyUnits(this); }
	CCircuitUnit* GetFriendlyUnit(springai::Unit* u);
	CCircuitUnit* GetFriendlyUnit(CCircuitUnit::Id unitId) { return allyTeam->GetFriendlyUnit(unitId); }
	const CAllyTeam::Units& GetFriendlyUnits() const { return allyTeam->GetFriendlyUnits(); }

private:
	CCircuitUnit* RegisterEnemyUnit(CCircuitUnit::Id unitId);
	void UnregisterEnemyUnit(CCircuitUnit* unit);
public:
	CCircuitUnit* GetEnemyUnit(springai::Unit* u) { return GetEnemyUnit(u->GetUnitId()); }
	CCircuitUnit* GetEnemyUnit(CCircuitUnit::Id unitId) { return allyTeam->GetEnemyUnit(unitId); }
	const CAllyTeam::Units& GetEnemyUnits() const { return allyTeam->GetEnemyUnits(); }

	CAllyTeam* GetAllyTeam() const { return allyTeam; }

private:
	CAllyTeam::Units teamUnits;  // owner
	CAllyTeam* allyTeam;
// ---- Units ---- END

// ---- AIOptions.lua ---- BEGIN
public:
	Difficulty GetDifficulty() const { return difficulty; }
	bool IsAllyAware() const { return allyAware; }
private:
	void InitOptions();
	Difficulty difficulty;
	bool allyAware;
// ---- AIOptions.lua ---- END

// ---- UnitDefs ---- BEGIN
private:
	struct cmp_str {
		bool operator()(char const* a, char const* b) {
			return strcmp(a, b) < 0;
		}
	};
public:
	using CircuitDefs = std::unordered_map<CCircuitDef::Id, CCircuitDef*>;
	using NamedDefs = std::map<const char*, CCircuitDef*, cmp_str>;

	CircuitDefs& GetCircuitDefs() { return defsById; }
	CCircuitDef* GetCircuitDef(const char* name);
	CCircuitDef* GetCircuitDef(CCircuitDef::Id unitDefId);
private:
	void InitUnitDefs();
	CircuitDefs  defsById;  // owner
	NamedDefs defsByName;
// ---- UnitDefs ---- END

public:
	bool IsInitialized() const { return initialized; }
	CGameAttribute* GetGameAttribute() const { return gameAttribute.get(); }
	std::shared_ptr<CScheduler>& GetScheduler() { return scheduler; }
	int GetLastFrame() const    { return lastFrame; }
	int GetSkirmishAIId() const { return skirmishAIId; }
	int GetTeamId() const       { return teamId; }
	int GetAllyTeamId() const   { return allyTeamId; }
	springai::OOAICallback* GetCallback() const   { return callback; }
	springai::Log*          GetLog() const        { return log.get(); }
	springai::Game*         GetGame() const       { return game.get(); }
	springai::Map*          GetMap() const        { return map.get(); }
	springai::Pathing*      GetPathing() const    { return pathing.get(); }
	springai::Drawer*       GetDrawer() const     { return drawer.get(); }
	springai::SkirmishAI*   GetSkirmishAI() const { return skirmishAI.get(); }
	springai::Team*         GetTeam() const       { return team.get(); }
	CSetupManager*    GetSetupManager() const    { return setupManager.get(); }
	CMetalManager*    GetMetalManager() const    { return metalManager.get(); }
	CEnergyGrid*      GetEnergyGrid() const      { return energyLink.get(); }
	CTerrainManager*  GetTerrainManager() const  { return terrainManager.get(); }
	CBuilderManager*  GetBuilderManager() const  { return builderManager.get(); }
	CFactoryManager*  GetFactoryManager() const  { return factoryManager.get(); }
	CEconomyManager*  GetEconomyManager() const  { return economyManager.get(); }
	CMilitaryManager* GetMilitaryManager() const { return militaryManager.get(); }

private:
	// debug
//	void DrawClusters();

	bool initialized;
	int lastFrame;
	int skirmishAIId;
	int teamId;
	int allyTeamId;
	SSkirmishAICallback* skirmishCallback;
	springai::OOAICallback*               callback;
	std::unique_ptr<springai::Log>        log;
	std::unique_ptr<springai::Game>       game;
	std::unique_ptr<springai::Map>        map;
	std::unique_ptr<springai::Pathing>    pathing;
	std::unique_ptr<springai::Drawer>     drawer;
	std::unique_ptr<springai::SkirmishAI> skirmishAI;
	std::unique_ptr<springai::Team>       team;

	static std::unique_ptr<CGameAttribute> gameAttribute;
	static unsigned int gaCounter;
	void CreateGameAttribute();
	void DestroyGameAttribute();
	std::shared_ptr<CScheduler> scheduler;
	std::shared_ptr<CSetupManager> setupManager;
	std::shared_ptr<CMetalManager> metalManager;
	std::shared_ptr<CEnergyGrid> energyLink;
	std::shared_ptr<CTerrainManager> terrainManager;
	std::shared_ptr<CBuilderManager> builderManager;
	std::shared_ptr<CFactoryManager> factoryManager;
	std::shared_ptr<CEconomyManager> economyManager;
	std::shared_ptr<CMilitaryManager> militaryManager;
	std::list<std::shared_ptr<IModule>> modules;
};

} // namespace circuit

#endif // SRC_CIRCUIT_CIRCUIT_H_
