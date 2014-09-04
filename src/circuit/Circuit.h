/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include "ExternalAI/Interface/AISEvents.h"
#include "ExternalAI/Interface/AISCommands.h"

#include "OOAICallback.h"			// C++ wrapper
#include "SSkirmishAICallback.h"	// "direct" C API

#include <memory>
#include <vector>

namespace circuit {

#define ERROR_UNKNOWN		200
#define ERROR_INIT			(ERROR_UNKNOWN + EVENT_INIT)
#define ERROR_RELEASE		(ERROR_UNKNOWN + EVENT_RELEASE)
#define ERROR_UPDATE		(ERROR_UNKNOWN + EVENT_UPDATE)
#define LOG(fmt, ...)	GetLog()->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

class CGameAttribute;
class CScheduler;
struct Metal;

class CCircuit {
public:
	bool initialized;

	CCircuit(springai::OOAICallback* callback);
	virtual ~CCircuit();

	int Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	int Release(int reason);
	int Update(int frame);
	int Message(int playerId, const char* message);
	int UnitCreated(springai::Unit* unit, springai::Unit* builder);
	int UnitFinished(int unitId);
	int LuaMessage(const char* inData);

	int GetSkirmishAIId();
//	springai::OOAICallback* GetCallback();
	springai::Log*          GetLog();
	springai::Game*         GetGame();
	springai::Map*          GetMap();
	springai::Pathing*      GetPathing();
	springai::Drawer*       GetDrawer();

private:
	int skirmishAIId;
	springai::OOAICallback* callback;
	springai::Log* log;
	springai::Game* game;
	springai::Map* map;
	springai::Pathing* pathing;
	springai::Drawer* drawer;

	static std::unique_ptr<CGameAttribute> gameAttribute;
	static unsigned int gaCounter;

	static void CreateGameAttribute();
	static void DestroyGameAttribute();

	std::shared_ptr<CScheduler> scheduler;

	void RegisterUnit(springai::Unit* unit);
	void ClusterizeMetal();
	void DrawClusters();
};

} // namespace circuit

#endif // CIRCUIT_H_
