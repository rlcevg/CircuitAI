/*
 * SetupManager.h
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
#define SRC_CIRCUIT_STATIC_SETUPMANAGER_H_

#include "AIFloat3.h"

namespace circuit {

class CCircuitAI;
class CSetupData;
class CCircuitUnit;
class CAllyTeam;

class CSetupManager {
public:
	enum class StartPosType: char {METAL_SPOT = 0, MIDDLE = 1, RANDOM = 2};

	CSetupManager(CCircuitAI* circuit, CSetupData* setupData);
	virtual ~CSetupManager();
	void ParseSetupScript(const char* setupScript);

	bool HasStartBoxes() const;
	bool CanChooseStartPos() const;

	void PickStartPos(CCircuitAI* circuit, StartPosType type);
	void SetStartPos(const springai::AIFloat3& pos) { startPos = basePos = pos; }
	const springai::AIFloat3& GetStartPos() const { return startPos; }
	void SetBasePos(const springai::AIFloat3& pos) { basePos = pos; }
	const springai::AIFloat3& GetBasePos() const { return basePos; }

	CCircuitUnit* GetCommander() const;
	CAllyTeam* GetAllyTeam() const;

private:
	void FindCommander();

	CCircuitAI* circuit;
	CSetupData* setupData;

	int commanderId;
	springai::AIFloat3 startPos;
	springai::AIFloat3 basePos;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPMANAGER_H_
