/*
 * SetupManager.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef SETUPMANAGER_H_
#define SETUPMANAGER_H_

#include "Game/GameSetup.h"

#include <array>
#include <vector>

namespace circuit {

enum class BoxEdges: int {BOTTOM = 0, LEFT = 1, RIGHT = 2, TOP = 3};

//typedef std::array<float, 4> Box;
// 0 -> bottom
// 1 -> left
// 2 -> right
// 3 -> top
using Box = std::array<float, 4>;

class CSetupManager {
public:
	CSetupManager(std::vector<Box>& startBoxes,
				  CGameSetup::StartPosType startPosType = CGameSetup::StartPosType::StartPos_ChooseInGame);
	virtual ~CSetupManager();

	bool IsEmpty();
	CGameSetup::StartPosType GetStartPosType();

	const Box& operator[](int idx) const;

private:
	std::vector<Box> startBoxes;
	CGameSetup::StartPosType startPosType;
};

} // namespace circuit

#endif // SETUPMANAGER_H_
