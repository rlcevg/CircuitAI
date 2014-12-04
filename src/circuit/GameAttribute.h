/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef GAMEATTRIBUTE_H_
#define GAMEATTRIBUTE_H_

#include "RagMatrix.h"

#include <memory>
#include <vector>
#include <unordered_set>

namespace springai {
	class GameRulesParam;
	class Pathing;
}

namespace circuit {

class CCircuitAI;
class CSetupManager;
class CMetalManager;
class CScheduler;
class CGameTask;

class CGameAttribute {
public:
	enum class StartPosType: char {METAL_SPOT = 0, MIDDLE = 1, RANDOM = 2};

	CGameAttribute();
	virtual ~CGameAttribute();

	void SetGameEnd(bool value);
	bool IsGameEnd();
	void RegisterAI(CCircuitAI* circuit);
	void UnregisterAI(CCircuitAI* circuit);

	void ParseSetupScript(const char* setupScript, int width, int height);
	bool HasStartBoxes(bool checkEmpty = true);
	bool CanChooseStartPos();
	void PickStartPos(CCircuitAI* circuit, StartPosType type);
	CSetupManager& GetSetupManager();

	void ParseMetalSpots(const char* metalJson);
	void ParseMetalSpots(const std::vector<springai::GameRulesParam*>& metalParams);
	bool HasMetalSpots(bool checkEmpty = true);
	bool HasMetalClusters();
	void ClusterizeMetalFirst(std::shared_ptr<CScheduler> scheduler, float maxDistance, int pathType, springai::Pathing* pathing);
	void ClusterizeMetal(std::shared_ptr<CScheduler> scheduler, float maxDistance, int pathType, springai::Pathing* pathing);
	CMetalManager& GetMetalManager();

private:
	bool gameEnd;
	std::unordered_set<CCircuitAI*> circuits;
	std::shared_ptr<CSetupManager> setupManager;
	std::shared_ptr<CMetalManager> metalManager;

	struct {
		int i;
		std::shared_ptr<CRagMatrix> matrix;
		springai::Pathing* pathing;
		int pathType;
		std::weak_ptr<CScheduler> schedWeak;
		float maxDistance;
		std::shared_ptr<CGameTask> task;
	} tmpDistStruct;
	void FillDistMatrix();
};

} // namespace circuit

#endif // GAMEATTRIBUTE_H_
