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
	class Game;
	class Map;
}

namespace circuit {

union Box {
	struct {
		float bottom;
		float left;
		float right;
		float top;
	};
	float edge[4];
};

class CSetupManager {
public:
	CSetupManager(std::vector<Box>& startBoxes,
				  CGameSetup::StartPosType startPosType = CGameSetup::StartPosType::StartPos_ChooseInGame);
	virtual ~CSetupManager();

	bool IsEmpty();
	bool CanChoosePos();
	void PickStartPos(springai::Game* game, springai::Map* map);

	const Box& operator[](int idx) const;

private:
	std::vector<Box> startBoxes;
	CGameSetup::StartPosType startPosType;
};

} // namespace circuit

#endif // SETUPMANAGER_H_
