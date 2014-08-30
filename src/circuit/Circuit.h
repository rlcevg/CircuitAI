/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUIT_H_
#define CIRCUIT_H_

//--- Delete
#include "MetalManager.h"
#include <vector>

#include "ExternalAI/Interface/AISEvents.h"
#include "ExternalAI/Interface/AISCommands.h"

#include "OOAICallback.h"			// C++ wrapper
#include "SSkirmishAICallback.h"	// "direct" C API

#include <memory>

namespace circuit {

#define ERROR_UNKNOWN		200
#define ERROR_INIT			(ERROR_UNKNOWN + EVENT_INIT)
#define ERROR_RELEASE		(ERROR_UNKNOWN + EVENT_RELEASE)
#define ERROR_UPDATE		(ERROR_UNKNOWN + EVENT_UPDATE)

using Box = std::array<float, 4>;
class CGameAttribute;
class CScheduler;

class CCircuit {
public:
	bool initialized;

	CCircuit(springai::OOAICallback* callback);
	virtual ~CCircuit();

	int Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	int Release(int reason);
	int Update(int frame);
	int Message(int playerId, const char* message);
	int LuaMessage(const char* inData);

private:
	int skirmishAIId;
	springai::OOAICallback* callback;
	springai::Log* log;
	springai::Game* game;
	springai::Map* map;
	springai::Pathing* pathing;

	static std::unique_ptr<CGameAttribute> gameAttribute;
	static unsigned int gaCounter;

	static void CreateGameAttribute();
	static void DestroyGameAttribute();

	std::shared_ptr<CScheduler> scheduler;

	void PickStartPos(const Box& box);
	void ParseEngineMetalSpots();
	void Clusterize(const std::vector<Metal>& spots);
	void ClearMetalClusters(std::vector<std::vector<Metal>>& metalCluster, std::vector<springai::AIFloat3>& centroids);
	void DrawConvexHulls(const std::vector<std::vector<Metal>>& metalCluster);
	void DrawCentroids(const std::vector<std::vector<Metal>>& metalCluster, const std::vector<springai::AIFloat3>& centroids);
	void DrawClusters();
};

} // namespace circuit

#endif // CIRCUIT_H_
