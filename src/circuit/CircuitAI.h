/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_CIRCUIT_H_
#define SRC_CIRCUIT_CIRCUIT_H_

#include "unit/AllyTeam.h"

#include <memory>
#include <map>
#include <list>
#include <vector>
#include <chrono>

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

using clock = std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

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
class CMetalManager;
class CTerrainManager;
class CBuilderManager;
class CFactoryManager;
class CEconomyManager;
class CMilitaryManager;
class CScheduler;
class CAllyTeam;
class CCircuitUnit;
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

public:
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

	CCircuitUnit* RegisterTeamUnit(CCircuitUnit::Id unitId);
	void UnregisterTeamUnit(CCircuitUnit* unit);
	CCircuitUnit* GetTeamUnit(CCircuitUnit::Id unitId);
	const std::map<CCircuitUnit::Id, CCircuitUnit*>& GetTeamUnits() const;
	void UpdateAllyUnits();
	CCircuitUnit* GetFriendlyUnit(springai::Unit* u);
	CCircuitUnit* GetFriendlyUnit(CCircuitUnit::Id unitId);
	CCircuitUnit* GetAllyUnit(CCircuitUnit::Id unitId);
	const std::map<CCircuitUnit::Id, CCircuitUnit*>& GetAllyUnits() const;
	CCircuitUnit* RegisterEnemyUnit(CCircuitUnit::Id unitId);
	void UnregisterEnemyUnit(CCircuitUnit* unit);
	CCircuitUnit* GetEnemyUnit(CCircuitUnit::Id unitId);
	const std::map<CCircuitUnit::Id, CCircuitUnit*>& GetEnemyUnits() const;

	bool IsUpdateTimeValid();

	void InitOptions();
	Difficulty GetDifficulty();
	bool IsAllyAware();

// ---- UnitDefs PROXY ---- BEGIN
public:
	CAllyTeam::CircuitDefs& GetCircuitDefs();
	CCircuitDef* GetCircuitDef(const char* name);
	CCircuitDef* GetCircuitDef(CCircuitDef::Id unitDefId);
private:
	CAllyTeam::CircuitDefs*  defsById;
	CAllyTeam::NCircuitDefs* defsByName;
// ---- UnitDefs PROXY ---- END

public:
	bool IsInitialized();
	CGameAttribute* GetGameAttribute();
	std::shared_ptr<CScheduler>& GetScheduler();
	int GetLastFrame();
	int GetSkirmishAIId();
	int GetTeamId();
	int GetAllyTeamId();
	springai::OOAICallback* GetCallback();
	springai::Log*          GetLog();
	springai::Game*         GetGame();
	springai::Map*          GetMap();
	springai::Pathing*      GetPathing();
	springai::Drawer*       GetDrawer();
	springai::SkirmishAI*   GetSkirmishAI();
	CSetupManager* GetSetupManager();
	CMetalManager* GetMetalManager();
	CTerrainManager* GetTerrainManager();
	CBuilderManager* GetBuilderManager();
	CFactoryManager* GetFactoryManager();
	CEconomyManager* GetEconomyManager();
	CMilitaryManager* GetMilitaryManager();

private:
	// debug
//	void DrawClusters();

	bool initialized;
	int lastFrame;
	int skirmishAIId;
	int teamId;
	int allyTeamId;
	springai::OOAICallback*               callback;
	std::unique_ptr<springai::Log>        log;
	std::unique_ptr<springai::Game>       game;
	std::unique_ptr<springai::Map>        map;
	std::unique_ptr<springai::Pathing>    pathing;
	std::unique_ptr<springai::Drawer>     drawer;
	std::unique_ptr<springai::SkirmishAI> skirmishAI;

	static std::unique_ptr<CGameAttribute> gameAttribute;
	static unsigned int gaCounter;
	void CreateGameAttribute();
	void DestroyGameAttribute();
	std::shared_ptr<CScheduler> scheduler;
	std::shared_ptr<CSetupManager> setupManager;
	std::shared_ptr<CMetalManager> metalManager;
	std::shared_ptr<CTerrainManager> terrainManager;
	std::shared_ptr<CBuilderManager> builderManager;
	std::shared_ptr<CFactoryManager> factoryManager;
	std::shared_ptr<CEconomyManager> economyManager;
	std::shared_ptr<CMilitaryManager> militaryManager;
	std::list<std::shared_ptr<IModule>> modules;

	int lastAllyUpdate;  // frame
	std::map<int, CCircuitUnit*> teamUnits;  // owner
	CAllyTeam* allyTeam;
	std::map<int, CCircuitUnit*> allyUnits;  // owner
	std::map<int, CCircuitUnit*> enemyUnits;  // owner

	clock::time_point startUpdate;

	// AIOptions.lua
	Difficulty difficulty;
	bool allyAware;
};

} // namespace circuit

#endif // SRC_CIRCUIT_CIRCUIT_H_
