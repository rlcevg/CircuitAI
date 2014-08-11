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

#include "OOAICallback.h"
#include "SSkirmishAICallback.h"

namespace circuit {

#define ERROR_UNKNOWN		200
#define ERROR_INIT			(ERROR_UNKNOWN + EVENT_INIT)
#define ERROR_RELEASE		(ERROR_UNKNOWN + EVENT_RELEASE)
#define ERROR_UPDATE		(ERROR_UNKNOWN + EVENT_UPDATE)

class CCircuit {
public:
	bool initialized;

	CCircuit(springai::OOAICallback* callback);
	virtual ~CCircuit();

	int Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	int Release(int reason);
	int Update(int frame);
	int LuaMessage(const char* inData);

private:
	springai::OOAICallback* callback;
	springai::Log* log;
	springai::Game* game;
	springai::Map* map;
};

} // namespace circuit

#endif // CIRCUIT_H_
