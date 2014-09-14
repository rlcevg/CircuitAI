/*
 * SetupManager.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef SETUPMANAGER_H_
#define SETUPMANAGER_H_

#include "Game/GameSetup.h"

#include <vector>

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CSetupManager {
public:
	union Box {
		struct {
			float bottom;
			float left;
			float right;
			float top;
		};
		float edge[4];

		bool ContainsPoint(const springai::AIFloat3& point) const;
	};

public:
	CSetupManager(std::vector<Box>& startBoxes,
				  CGameSetup::StartPosType startPosType = CGameSetup::StartPosType::StartPos_ChooseInGame);
	virtual ~CSetupManager();

	bool IsEmpty();
	bool CanChooseStartPos();
	int GetNumAllyTeams();

	const Box& operator[](int idx) const;

private:
	std::vector<Box> startBoxes;
	CGameSetup::StartPosType startPosType;
};

} // namespace circuit

#endif // SETUPMANAGER_H_
