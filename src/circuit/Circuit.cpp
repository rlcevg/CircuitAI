/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "Circuit.h"
#include "StartBox.h"
#include "utils.h"

#include "ExternalAI/Interface/AISCommands.h"

#include "Game.h"
#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Log.h"

#include "AIFloat3.h"

#include <regex>

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
	Release(0);
}

void CCircuit::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
{
	CStartBox::CreateInstance(game->GetSetupScript(), map->GetWidth(), map->GetHeight());

	const float* box = startBox[game->GetMyAllyTeam()];
	float x = (box[static_cast<int>(BoxEdges::LEFT)] + box[static_cast<int>(BoxEdges::RIGHT)] ) / 2.0;
	float z = (box[static_cast<int>(BoxEdges::BOTTOM)] + box[static_cast<int>(BoxEdges::TOP)] ) / 2.0;

	game->SendStartPosition(false, AIFloat3(x, 0.0, z));

	initialized = true;
}

void CCircuit::Release(int reason)
{
	if (!initialized) {
		return;
	}

	CStartBox::DestroyInstance();

	initialized = false;
}

void CCircuit::Update(int frame)
{
	if (frame == 300) {
		LOG("HIT 300 frame");
		std::vector<springai::Unit*> units = callback->GetTeamUnits();
		if (units.size() > 0) {
			LOG("found mah comm");
			Unit* commander = units.front();
			Unit* friendCommander = NULL;;
			std::vector<springai::Unit*> friendlies = callback->GetFriendlyUnits();
			for (Unit* unit : friendlies) {
				UnitDef* unitDef = unit->GetDef();
				if (strcmp(unitDef->GetName(), "armcom1") == 0) {
					if (commander->GetUnitId() != unit->GetUnitId()) {
						LOG("found friendly comm");
						friendCommander = unit;
						break;
					}
					LOG("found some comm");
				}
			}

			if (friendCommander) {
				LOG("giving guard order");
				commander->Guard(friendCommander);
//				commander->Build(callback->GetUnitDefByName("armsolar"), commander->GetPos(), UNIT_COMMAND_BUILD_NO_FACING);
			}
		}
	}
}

} // namespace circuit
