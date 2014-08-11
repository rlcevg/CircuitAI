/*
 * Circuit.cpp
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#include "Circuit.h"
#include "StartBox.h"
#include "MetalSpot.h"
#include "utils.h"

#include "Game.h"
#include "Map.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Log.h"

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
	Release(0);
}

int CCircuit::Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback)
{
	CStartBox::CreateInstance(game->GetSetupScript(), map->GetWidth(), map->GetHeight());

	if (startBoxes.IsEmpty()) {
		CStartBox::DestroyInstance();
		return ERROR_INIT;
	}

	const float* box = startBoxes[game->GetMyAllyTeam()];
	float x = (box[static_cast<int>(BoxEdges::LEFT)] + box[static_cast<int>(BoxEdges::RIGHT)] ) / 2.0;
	float z = (box[static_cast<int>(BoxEdges::BOTTOM)] + box[static_cast<int>(BoxEdges::TOP)] ) / 2.0;

	game->SendStartPosition(false, AIFloat3(x, 0.0, z));

	initialized = true;
	// signal: everything went OK
	return 0;
}

int CCircuit::Release(int reason)
{
	CStartBox::DestroyInstance();

	if (!initialized) {
		return ERROR_RELEASE;
	}

	initialized = false;
	// signal: everything went OK
	return 0;
}

int CCircuit::Update(int frame)
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

	// signal: everything went OK
	return 0;
}

int CCircuit::LuaMessage(const char* inData)
{
	LOG(inData);
	if (strncmp(inData, "METAL_SPOTS:", 12) == 0) {
		LOG("YES");
	}
//		String json = inData.substring(12);
//	JsonParserFactory factory=JsonParserFactory.getInstance();
//	JSONParser parser=factory.newJsonParser();
//	ArrayList<HashMap> jsonData=(ArrayList)parser.parseJson(json).values().toArray()[0];
//	initializeGraph(jsonData);
//	parent.debug("Parsed JSON metalmap with "+metalSpots.size()+" spots and "+links.size()+" links");
//	Set<Integer> enemies = parent.getEnemyAllyTeamIDs();
//	for(int enemy:enemies){
//	float[] box = parent.getEnemyBox(enemy);
//	if(box!=null){
//	// 0 -> bottom
//	// 1 -> left
//	// 2 -> right
//	// 3 -> top
//	for (MetalSpot ms:metalSpots){
//	AIFloat3 pos = ms.position;
//	if(pos.z > box[3] && pos.z < box[0] && pos.x>box[1] && pos.x<box[2]){
//	ms.hostile = true;
//	ms.setShadowCaptured(true);
//	for(Link l:ms.links){
//	l.contested = true;
//	}
//	}
//	}
//	}
//	}
//	}
	return 0; //signaling: OK
}

} // namespace circuit
