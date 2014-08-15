/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "Circuit.h"
#include "GameAttribute.h"
#include "utils.h"

#include "Game.h"
#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Log.h"

// ------------ delete begin
#include "Drawer.h"
#include "Resource.h"
#include "AIColor.h"
#include "Debug.h"
#include "GraphDrawer.h"
#include "GraphLine.h"
// ------------ delete end

#include "AIFloat3.h"

#include <string.h>

namespace circuit {

using namespace springai;

#define LOG(fmt, ...)	log->DoLog(utils::string_format(std::string(fmt), ##__VA_ARGS__).c_str())

CCircuit::CCircuit(springai::OOAICallback* callback) :
		initialized(false),
		callback(callback),
		log(callback->GetLog()),
		game(callback->GetGame()),
		map(callback->GetMap())
{
}

CCircuit::~CCircuit()
{
	if (initialized) {
		Release(0);
	}
}

int CCircuit::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
{
	CGameAttribute::CreateInstance();

	// TODO: parse setupScript to get StartPosType
	//CGameSetup::StartPos_ChooseInGame
	gameAttribute.parseSetupScript(game->GetSetupScript(), map->GetWidth(), map->GetHeight());
	CStartBox& startBoxes = gameAttribute.GetStartBoxes();
	if (startBoxes.IsEmpty()) {
		CGameAttribute::DestroyInstance();
		return ERROR_INIT;
	}

	if (startBoxes.startPosType == CGameSetup::StartPos_ChooseInGame) {
		CalcStartPos(startBoxes[game->GetMyAllyTeam()]);
	}

	GraphDrawer* drawer = callback->GetDebug()->GetGraphDrawer();
	drawer->SetPosition(-0.6f, -0.6f);
	drawer->SetSize(0.8f, 0.8f);
	GraphLine* line = drawer->GetGraphLine();
	line->SetColor(0, AIColor(0.9, 0.1, 0.1));
	line->SetLabel(0, "label");

	initialized = true;
	// signal: everything went OK
	return 0;
}

int CCircuit::Release(int reason)
{
	CGameAttribute::DestroyInstance();

	initialized = false;
	// signal: everything went OK
	return 0;
}

int CCircuit::Update(int frame)
{
	if (frame % 120 == 0) {
//		LOG("HIT 300 frame");
		std::vector<springai::Unit*> units = callback->GetTeamUnits();
		if (units.size() > 0) {
//			LOG("found mah comm");
			Unit* commander = units.front();
			Unit* friendCommander = NULL;;
			std::vector<springai::Unit*> friendlies = callback->GetFriendlyUnits();
			for (Unit* unit : friendlies) {
				UnitDef* unitDef = unit->GetDef();
				if (strcmp(unitDef->GetName(), "armcom1") == 0) {
					if (commander->GetUnitId() != unit->GetUnitId()) {
//						LOG("found friendly comm");
						friendCommander = unit;
						break;
					} else {
//						LOG("found mah comm again");
					}
				}
			}

			if (friendCommander) {
				LOG("giving guard order");
				commander->Guard(friendCommander);
//				commander->Build(callback->GetUnitDefByName("armsolar"), commander->GetPos(), UNIT_COMMAND_BUILD_NO_FACING);
				AIFloat3 pos1 = commander->GetPos();
				pos1.y += 10;
				AIFloat3 pos2 = friendCommander->GetPos();
				pos2.y += 10;
				map->GetDrawer()->AddLine(pos1, pos2);
				map->GetDrawer()->AddNotification(commander->GetPos(), AIColor(0.9, 0.1, 0.1), 200);
			}
			map->GetDrawer()->AddPoint(commander->GetPos(), "Pinh!");

			GraphLine* line = callback->GetDebug()->GetGraphDrawer()->GetGraphLine();
			line->AddPoint(0, -0.9, -0.9);
			line->AddPoint(0, 0.9, 0.9);
		}
	}

	// signal: everything went OK
	return 0;
}

int CCircuit::Message(int playerId, const char* message)
{
	if (strncmp(message, "~стройсь", 10) == 0) {
		CalcStartPos(gameAttribute.GetStartBoxes()[game->GetMyAllyTeam()]);
	}
	return 0; //signaling: OK
}

int CCircuit::LuaMessage(const char* inData)
{
	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
		LOG(inData + 12);
		gameAttribute.parseMetalSpots(inData + 12);
//		LOG("size1: %i", gameAttribute.GetMetalSpots().size());

		std::vector<springai::Resource*> ress = callback->GetResources();
		for (auto& res : ress) {
			LOG(res->GetName());
		}

		std::vector<springai::AIFloat3> spots = map->GetResourceMapSpotsPositions(callback->GetResourceByName("Metal"));
		for (auto& spot : spots) {
			LOG("x:%f, z:%f, income:%f", spot.x, spot.z, spot.y);
		}
		LOG("size2: %i", spots.size());
	}
	return 0; //signaling: OK
}

void CCircuit::CalcStartPos(const Box& box)
{
//	float x = (box[static_cast<int>(BoxEdges::LEFT)] + box[static_cast<int>(BoxEdges::RIGHT)] ) / 2.0;
//	float z = (box[static_cast<int>(BoxEdges::BOTTOM)] + box[static_cast<int>(BoxEdges::TOP)] ) / 2.0;
	int min, max, output;
	min = box[static_cast<int>(BoxEdges::LEFT)];
	max = box[static_cast<int>(BoxEdges::RIGHT)];
	output = min + (rand() % (int)(max - min + 1));
	float x = output;
	min = box[static_cast<int>(BoxEdges::TOP)];
	max = box[static_cast<int>(BoxEdges::BOTTOM)];
	output = min + (rand() % (int)(max - min + 1));
	float z = output;

	game->SendStartPosition(false, AIFloat3(x, 0.0, z));
}

} // namespace circuit
