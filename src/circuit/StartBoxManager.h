/*
 * StartBoxManager.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef STARTBOXMANAGER_H_
#define STARTBOXMANAGER_H_

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

class CStartBoxManager {
public:
	CStartBoxManager(std::vector<Box>& startBoxes,
					 CGameSetup::StartPosType startPosType = CGameSetup::StartPosType::StartPos_ChooseInGame);
	virtual ~CStartBoxManager();

	bool IsEmpty();
	CGameSetup::StartPosType GetStartPosType();

	const Box& operator[](int idx) const;

private:
	std::vector<Box> startBoxes;
	CGameSetup::StartPosType startPosType;
};

} // namespace circuit

#endif // STARTBOXMANAGER_H_
