/*
 * SetupManager.cpp
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#include "SetupManager.h"
#include "Circuit.h"

#include "Game.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CSetupManager::CSetupManager(std::vector<Box>& startBoxes, CGameSetup::StartPosType startPosType) :
		startBoxes(startBoxes),
		startPosType(startPosType)
{
}

CSetupManager::~CSetupManager()
{
}

bool CSetupManager::IsEmpty()
{
	return startBoxes.empty();
}

bool CSetupManager::CanChoosePos()
{
	return startPosType == CGameSetup::StartPos_ChooseInGame;
}

void CSetupManager::PickStartPos(Game* game, Map* map)
{
	Box& box = startBoxes[game->GetMyAllyTeam()];
	int min, max;
	min = box.left;
	max = box.right;
	float x = min + (rand() % (int)(max - min + 1));
	min = box.top;
	max = box.bottom;
	float z = min + (rand() % (int)(max - min + 1));

	game->SendStartPosition(false, AIFloat3(x, map->GetElevationAt(x, z), z));
}

const Box& CSetupManager::operator[](int idx) const
{
	return startBoxes[idx];
}

} // namespace circuit
