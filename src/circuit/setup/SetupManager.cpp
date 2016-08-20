/*
 * SetupManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "setup/SetupManager.h"
#include "setup/SetupData.h"
#include "resource/MetalManager.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/Scheduler.h"
#include "util/utils.h"
#include "json/json.h"

#include "OOAICallback.h"
#include "Game.h"
#include "Map.h"
#include "DataDirs.h"
#include "File.h"
#include "Log.h"
#include "Lua.h"

#include <regex>

namespace circuit {

using namespace springai;

CSetupManager::CSetupManager(CCircuitAI* circuit, CSetupData* setupData)
		: circuit(circuit)
		, setupData(setupData)
		, config(nullptr)
		, commanderId(-1)
		, startPos(-RgtVector)
		, basePos(-RgtVector)
		, emptyShield(0.f)
		, commChoice(nullptr)
		, morphFrame(-1)
{
	const char* setupScript = circuit->GetGame()->GetSetupScript();
	if (!setupData->IsInitialized()) {
		setupData->ParseSetupScript(circuit, setupScript);
	}
	DisabledUnits(setupScript);
	circuit->GetScheduler()->RunTaskAt(std::make_shared<CGameTask>(&CSetupManager::FindCommander, this));
}

CSetupManager::~CSetupManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete config;
}

void CSetupManager::DisabledUnits(const char* setupScript)
{
	std::string script(setupScript);
	std::regex patternDisabled("disabledunits=(.*);", std::regex::ECMAScript | std::regex::icase);
	std::string::const_iterator start = script.begin();
	std::string::const_iterator end = script.end();
	std::smatch section;
	if (std::regex_search(start, end, section, patternDisabled)) {
		// !setoptions disabledunits=raveparty+zenith+mahlaze
		std::string opt_str = section[1];
		start = opt_str.begin();
		end = opt_str.end();
		std::regex patternUnit("\\w+");
		while (std::regex_search(start, end, section, patternUnit)) {
			CCircuitDef* cdef = circuit->GetCircuitDef(std::string(section[0]).c_str());
			if (cdef != nullptr) {
				cdef->SetMaxThisUnit(0);
			}
			start = section[0].second;
		}
	}
}

bool CSetupManager::OpenConfig(const std::string& cfgName)
{
	std::string cfgDefault;
	if (cfgName.empty()) {
		/*
		 * Try startscript specific config
		 */
		configName = "startscript";
		std::string strJson = setupData->GetConfigJson(circuit->GetSkirmishAIId());
		if (!strJson.empty()) {
			config = ParseConfig(strJson.c_str());

			if (config != nullptr) {
				return true;
			}
		}

		/*
		 * Try game specific config
		 */
		Map* map = circuit->GetMap();
		std::string filename = "LuaRules/Configs/CircuitAI/";
		configName = utils::MakeFileSystemCompatible(map->GetName()) + ".json";
		filename += configName;

		const char* cfgJson = ReadConfig(filename);
		if (cfgJson != nullptr) {
			config = ParseConfig(cfgJson);
			delete[] cfgJson;

			if (config != nullptr) {
				return true;
			}
		}

		/*
		 * Try default game specific config
		 */
		cfgDefault = "circuit";
		filename = "LuaRules/Configs/CircuitAI/Default/";
		configName = cfgDefault + ".json";
		filename += configName;

		cfgJson = ReadConfig(filename);
		if (cfgJson != nullptr) {
			config = ParseConfig(cfgJson);
			delete[] cfgJson;

			if (config != nullptr) {
				return true;
			}
		}

	} else {
		cfgDefault = cfgName;
	}

	/*
	 * Locate default config
	 */
	std::string filename("config" SLASH);
	configName = (cfgDefault.find(".json") == std::string::npos) ? (cfgDefault + ".json") : cfgDefault;
	filename += configName;
	if (!LocatePath(filename)){
		circuit->LOG("Config file is missing! (%s)", configName.c_str());
		return false;
	}

	const char* cfgJson = ReadConfig(filename);
	if (cfgJson == nullptr) {
		return false;
	}

	config = ParseConfig(cfgJson);
	delete[] cfgJson;

	return (config != nullptr);
}

void CSetupManager::CloseConfig()
{
	delete config;
	config = nullptr;
}

bool CSetupManager::HasStartBoxes() const
{
	return setupData->IsInitialized();
}

bool CSetupManager::CanChooseStartPos() const
{
	return setupData->CanChooseStartPos();
}

void CSetupManager::PickStartPos(CCircuitAI* circuit, StartPosType type)
{
	float x, z;
	CAllyTeam* allyTeam = circuit->GetAllyTeam();
	const CAllyTeam::SBox& box = allyTeam->GetStartBox();

	auto random = [](const CAllyTeam::SBox& box, float& x, float& z) {
		int min, max;
		min = box.left;
		max = box.right;
		x = min + (rand() % (int)(max - min + 1));
		min = box.top;
		max = box.bottom;
		z = min + (rand() % (int)(max - min + 1));
	};

	switch (type) {
		case StartPosType::METAL_SPOT: {
			const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
			const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
			CTerrainManager* terrainManager = circuit->GetTerrainManager();
			STerrainMapMobileType* mobileType = terrainManager->GetMobileTypeById(commChoice->GetMobileId());
			Lua* lua = circuit->GetCallback()->GetLua();

			std::map<int, CMetalData::MetalIndices> validPoints;
			for (unsigned idx = 0; idx < clusters.size(); ++idx) {
				for (int i : clusters[idx].idxSpots) {
					std::string cmd("ai_is_valid_startpos:");
					const CMetalData::SMetal& spot = spots[i];
					cmd += utils::int_to_string(spot.position.x) + "/" + utils::int_to_string(spot.position.z);
					std::string result = lua->CallRules(cmd.c_str(), cmd.size());
					if (result != "1") {
						continue;
					}

					int iS = terrainManager->GetSectorIndex(spots[i].position);
					STerrainMapArea* area = mobileType->sector[iS].area;
					if ((area != nullptr) && area->areaUsable) {
						validPoints[idx].push_back(i);
					}
				}
			}

			delete lua;
			if (!validPoints.empty()) {
				struct SCluster {
					unsigned count;
					float distDivIncome;
				};
				const AIFloat3 center(terrainManager->GetTerrainWidth() / 2, 0, terrainManager->GetTerrainHeight() / 2);
				std::vector<std::pair<int, SCluster>> validClusters;
				for (auto& kv : validPoints) {
					SCluster c;
					c.count = allyTeam->GetClusterTeam(kv.first).count;
					const CMetalData::SCluster& cl = clusters[kv.first];
					const float income = cl.income + (float)rand() / RAND_MAX - 0.5f;
					c.distDivIncome = center.distance(cl.geoCentr) / income;
					validClusters.push_back(std::make_pair(kv.first, c));
				}
				std::random_shuffle(validClusters.begin(), validClusters.end());

				auto cmp = [&clusters](const std::pair<int, SCluster>& a, const std::pair<int, SCluster>& b) {
					if (a.second.count < b.second.count) {
						return true;
					} else if (a.second.count > b.second.count) {
						return false;
					}
					return a.second.distDivIncome < b.second.distDivIncome;
				};
				std::sort(validClusters.begin(), validClusters.end(), cmp);

				int clusterId = validClusters.front().first;
				allyTeam->OccupyCluster(clusterId, circuit->GetTeamId());

				const CMetalData::MetalIndices& indices = validPoints[clusterId];
				const CMetalData::SMetal& spot = spots[indices[rand() % indices.size()]];
				x = spot.position.x;
				z = spot.position.z;
				break;
			}

//			AIFloat3 posFrom(box.left, 0, box.top);
//			AIFloat3 posTo(box.right, 0, box.bottom);
//			CMetalManager* metalManager = circuit->GetMetalManager();
//			CMetalData::MetalIndices inBoxIndices = metalManager->FindWithinRangeSpots(posFrom, posTo);
//			if (!inBoxIndices.empty()) {
//				const CMetalData::Metals& spots = metalManager->GetSpots();
//				CTerrainManager* terrainManager = circuit->GetTerrainManager();
//				STerrainMapMobileType* mobileType = terrainManager->GetMobileTypeById(commChoice->GetMobileId());
//				std::vector<int> filteredIndices;
//				for (auto idx : inBoxIndices) {
//					int iS = terrainManager->GetSectorIndex(spots[idx].position);
//					STerrainMapArea* area = mobileType->sector[iS].area;
//					if ((area != nullptr) && area->areaUsable) {
//						filteredIndices.push_back(idx);
//					}
//				}
//				if (!filteredIndices.empty()) {
//					const AIFloat3& pos = spots[filteredIndices[rand() % filteredIndices.size()]].position;
//					x = pos.x;
//					z = pos.z;
//					break;
//				}
//			}

			random(box, x, z);
			break;
		}
		case StartPosType::MIDDLE: {
			x = (box.left + box.right) / 2;
			z = (box.top + box.bottom) / 2;
			break;
		}
		case StartPosType::RANDOM:
		default: {
			random(box, x, z);
			break;
		}
	}

	AIFloat3 pos = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	SetStartPos(pos);
	circuit->GetGame()->SendStartPosition(false, pos);
}

bool CSetupManager::PickCommander()
{
	std::vector<CCircuitDef*> comms;
	float bestPower = .0f;

	if (commChoice == nullptr) {
		const CCircuitAI::CircuitDefs& defs = circuit->GetCircuitDefs();
		for (auto& kv : defs) {
			CCircuitDef* cdef = kv.second;

			std::string lvl1 = cdef->GetUnitDef()->GetName();
			if ((lvl1.find("dyntrainer_") != 0) || (lvl1.find("_base") != lvl1.size() - 5)) {
				continue;
			}

			const std::map<std::string, std::string>& customParams = cdef->GetUnitDef()->GetCustomParams();
			auto it = customParams.find("level");
			if ((it == customParams.end()) || (utils::string_to_int(it->second) != 1)) {
				continue;
			}
			comms.push_back(cdef);

			if (bestPower < cdef->GetBuildDistance()) {  // No more UnitDef->GetAutoHeal() :(
				bestPower = cdef->GetBuildDistance();
				commChoice = cdef;
			}
		}
		if (comms.empty()) {
			return false;
		}
	}

	std::string cmd("ai_commander:");
	cmd += ((commChoice == nullptr) ? comms[rand() % comms.size()] : commChoice)->GetUnitDef()->GetName();
	Lua* lua = circuit->GetCallback()->GetLua();
	lua->CallRules(cmd.c_str(), cmd.size());
	delete lua;

	return true;
}

CCircuitUnit* CSetupManager::GetCommander() const
{
	return circuit->GetTeamUnit(commanderId);
}

CAllyTeam* CSetupManager::GetAllyTeam() const
{
	return setupData->GetAllyTeam(circuit->GetAllyTeamId());
}

void CSetupManager::ReadConfig()
{
	const Json::Value& root = GetConfig();
	const std::string& cfgName = GetConfigName();
	const Json::Value& shield = root["retreat"]["shield"];
	emptyShield = shield.get((unsigned)0, 0.1f).asFloat();
	fullShield = shield.get((unsigned)1, 0.6f).asFloat();

	const Json::Value& comm = root["commander"];
	const Json::Value& items = comm["unit"];
	std::vector<CCircuitDef*> commChoices;
	commChoices.reserve(items.size());
	float magnitude = 0.f;
	std::vector<float> weight;
	weight.reserve(items.size());
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();

	for (const std::string& defName : items.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef(defName.c_str());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), defName.c_str());
			continue;
		}

		commChoices.push_back(cdef);

		const Json::Value& comm = items[defName];
		const float imp = comm.get("importance", 0.f).asFloat();
		magnitude += imp;
		weight.push_back(imp);

		morphFrame = comm.get("morph_after", -1).asInt();

		const Json::Value& strt = comm["start"];
		opener.reserve(strt.size());
		for (const Json::Value& role : strt) {
			auto it = roleNames.find(role.asString());
			if (it != roleNames.end()) {
				opener.push_back(it->second);
			}
		}
	}

	// FIXME: Tie upgrades to UnitDef
	const Json::Value& upgr = comm["upgrade"];
	modules.reserve(upgr.size());
	for (const Json::Value& lvl : upgr) {
		std::vector<float> mdls;
		for (const Json::Value& m : lvl) {
			mdls.push_back(m.asFloat());
		}
		modules.push_back(mdls);
	}

	if (!commChoices.empty()) {
		unsigned choice = 0;
		float dice = (float)rand() / RAND_MAX;
		float total = .0f;
		for (unsigned i = 0; i < weight.size(); ++i) {
			total += weight[i];
			if (dice < total) {
				choice = i;
				break;
			}
		}
		commChoice = commChoices[choice];
	}
}

void CSetupManager::FindCommander()
{
	std::vector<Unit*> units = circuit->GetCallback()->GetTeamUnits();
	for (Unit* u : units) {
		UnitDef* def = u->GetDef();
		bool valid = def->IsBuilder();
		delete def;
		if (valid) {
			commanderId = u->GetUnitId();
//			if (!utils::is_valid(startPos)) {
				SetStartPos(u->GetPos());
//			}
			break;
		}
	}
	utils::free_clear(units);
}

bool CSetupManager::LocatePath(std::string& filename)
{
	static const size_t absPath_sizeMax = 2048;
	char absPath[absPath_sizeMax];
	DataDirs* datadirs = circuit->GetCallback()->GetDataDirs();
	const bool dir = !filename.empty() && (*filename.rbegin() == '/' || *filename.rbegin() == '\\');
	const bool located = datadirs->LocatePath(absPath, absPath_sizeMax, filename.c_str(), false /*writable*/, false /*create*/, dir, false /*common*/);
	if (located) {
		filename = absPath;
	}
	delete datadirs;
	return located;
}

const char* CSetupManager::ReadConfig(const std::string& filename)
{
	File* file = circuit->GetCallback()->GetFile();
	int fileSize = file->GetSize(filename.c_str());
	if (fileSize <= 0) {
		circuit->LOG("Missing config file! (%s)", filename.c_str());
		delete file;
		return nullptr;
	}

	char* cfgJson = new char [fileSize + 1];
	file->GetContent(filename.c_str(), cfgJson, fileSize);
	cfgJson[fileSize] = 0;
	delete file;
	return cfgJson;
}

Json::Value* CSetupManager::ParseConfig(const char* cfgJson)
{
	Json::Value jsonAll;
	Json::Reader json;
	bool isOk = json.parse(cfgJson, jsonAll, false);
	if (!isOk) {
		circuit->LOG("Malformed config format! (%s)", configName.c_str());
		return nullptr;
	}

	const char* diffs[] = {setup::easy, setup::normal, setup::hard};
	const char* diffName = diffs[static_cast<size_t>(circuit->GetDifficulty())];
	Json::Value& jsonDiff = jsonAll[diffName];
	if (jsonDiff.isNull()) {
		circuit->LOG("Malformed difficulty! (%s : %s)", configName.c_str(), diffName);
		const std::string& diffDefault = jsonAll.get("default", "normal").asString();
		jsonDiff = jsonAll[diffDefault];
		if (jsonDiff.isNull()) {
			circuit->LOG("Malformed difficulty! (%s : %s)", configName.c_str(), diffDefault.c_str());
			return nullptr;
		}
	}

	Json::Value* cfg = new Json::Value;
	*cfg = jsonDiff;
	return cfg;
}

} // namespace circuit
