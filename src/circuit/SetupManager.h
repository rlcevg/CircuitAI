/*
 * SetupManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SETUPMANAGER_H_
#define SRC_CIRCUIT_SETUPMANAGER_H_

#include "AIFloat3.h"

namespace circuit {

class CCircuitAI;
class CSetupData;
class CCircuitUnit;

class CSetupManager {
public:
	enum class StartPosType: char {METAL_SPOT = 0, MIDDLE = 1, RANDOM = 2};

	CSetupManager(CCircuitAI* circuit, CSetupData* setupData);
	virtual ~CSetupManager();
	void ParseSetupScript(const char* setupScript, float width, float height);

	bool HasStartBoxes();
	bool CanChooseStartPos();

	void PickStartPos(CCircuitAI* circuit, StartPosType type);
	CCircuitUnit* GetCommander();
	void SetStartPos(const springai::AIFloat3& pos);
	const springai::AIFloat3& GetStartPos();

private:
	void FindCommander();

	CCircuitAI* circuit;
	CSetupData* setupData;

	int commanderId;
	springai::AIFloat3 startPos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SETUPMANAGER_H_
