/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include "StartBox.h"
#include "MetalSpot.h"

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
	int Message(int playerId, const char* message);
	int LuaMessage(const char* inData);

private:
	springai::OOAICallback* callback;
	springai::Log* log;
	springai::Game* game;
	springai::Map* map;

	void CalcStartPos(const Box& box);
	void Clusterize(const std::vector<Metal>& spots);
	void ClearMetalClusters(std::vector<std::vector<Metal>>& metalCluster, std::vector<springai::AIFloat3>& centroids);
	void DrawConvexHulls(const int nclusters, const int nrows, const int* clusterid, const std::vector<Metal>& spots,
			std::vector<std::vector<Metal>>& metalCluster);
	void DrawCentroids(const int ncluster, const double** cdata, const std::vector<std::vector<Metal>>& metalCluster,
			std::vector<springai::AIFloat3>& centroids);
};

} // namespace circuit

#endif // CIRCUIT_H_
