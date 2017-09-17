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
#include "OptionValues.h"
#include "SkirmishAI.h"
#include "Game.h"
#include "Map.h"
#include "DataDirs.h"
#include "File.h"
#include "Log.h"
#include "Lua.h"
#include "Info.h"

#include <regex>

namespace circuit {

using namespace springai;

CSetupManager::CSetupManager(CCircuitAI* circuit, CSetupData* setupData)
		: circuit(circuit)
		, setupData(setupData)
		, config(nullptr)
		, commander(nullptr)
		, startPos(-RgtVector)
		, basePos(-RgtVector)
		, emptyShield(0.f)
		, commChoice(nullptr)
{
	const char* setupScript = circuit->GetGame()->GetSetupScript();
	if (!setupData->IsInitialized()) {
		setupData->ParseSetupScript(circuit, setupScript);
	}
	DisabledUnits(setupScript);

	findStart = std::make_shared<CGameTask>(&CSetupManager::FindStart, this);
	circuit->GetScheduler()->RunTaskEvery(findStart, 1);
}

CSetupManager::~CSetupManager()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
	delete config;
}

void CSetupManager::DisabledUnits(const char* setupScript)
{
	std::string script(setupScript);
	std::regex patternModoptions("\\[modoptions\\]\\s*\\{([\\s\\S]+?(?=\\}\\s*(\\[|$)))", std::regex::ECMAScript | std::regex::icase);
	std::regex patternDisabled("disabledunits=(.*);", std::regex::ECMAScript | std::regex::icase);
	auto disableUnits = [this](const std::string& opt_str) {
		std::string::const_iterator start = opt_str.begin();
		std::string::const_iterator end = opt_str.end();
		std::regex patternUnit("\\w+");
		std::smatch section;
		while (std::regex_search(start, end, section, patternUnit)) {
			CCircuitDef* cdef = circuit->GetCircuitDef(std::string(section[0]).c_str());
			if (cdef != nullptr) {
				cdef->SetMaxThisUnit(0);
			}
			start = section[0].second;
		}
	};

	std::smatch modoptions;
	if (std::regex_search(script, modoptions, patternModoptions)) {
		std::string modoptionsBody = modoptions[1];
		std::smatch disabledunits;
		if (std::regex_search(modoptionsBody, disabledunits, patternDisabled)) {
			// !setoptions disabledunits=armwar+armpw+raveparty+zenith+mahlazer
			disableUnits(disabledunits[1]);
		}
	}

	OptionValues* options = circuit->GetSkirmishAI()->GetOptionValues();
	const char* value = options->GetValueByKey("disabledunits");
	delete options;
	if (value != nullptr) {
		disableUnits(value);
	}
}

bool CSetupManager::OpenConfig(const std::string& cfgName)
{
	bool isOk = LoadConfig(cfgName);
	if (isOk) {
		OverrideConfig();
	}
	return isOk;
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
	const std::string& prefix = comm["prefix"].asString();
	const std::string& suffix = comm["suffix"].asString();
	const Json::Value& items = comm["unit"];
	std::vector<CCircuitDef*> commChoices;
	commChoices.reserve(items.size());
	float magnitude = 0.f;
	std::vector<float> weight;
	weight.reserve(items.size());
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();

	for (const std::string& commName : items.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef((prefix + commName + suffix).c_str());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown commander '%s'", cfgName.c_str(), commName.c_str());
			continue;
		}

		commChoices.push_back(cdef);

		const Json::Value& comm = items[commName];
		const float imp = comm.get("importance", 0.f).asFloat();
		magnitude += imp;
		weight.push_back(imp);

		const Json::Value& strt = comm["start"];
		const Json::Value& defStrt = strt["default"];
		SStart& facStart = start[cdef->GetId()];
		facStart.defaultStart.reserve(defStrt.size());
		for (const Json::Value& role : defStrt) {
			auto it = roleNames.find(role.asString());
			if (it == roleNames.end()) {
				circuit->LOG("CONFIG %s: default start has unknown role '%s'", cfgName.c_str(), role.asCString());
			} else {
				facStart.defaultStart.push_back(it->second);
			}
		}
		const Json::Value& facStrt = strt["factory"];
		for (const std::string& defName : facStrt.getMemberNames()) {
			CCircuitDef* fdef = circuit->GetCircuitDef(defName.c_str());
			if (fdef == nullptr) {
				circuit->LOG("CONFIG %s: has unknown UnitDef '%s'", cfgName.c_str(), defName.c_str());
				continue;
			}
			const Json::Value& multiStrt = facStrt[defName];
			std::vector<SOpener>& facOpeners = facStart.openers[fdef->GetId()];
			facOpeners.reserve(multiStrt.size());
			for (const Json::Value& opener : multiStrt) {
				const float prob = opener.get((unsigned)0, 1.f).asFloat();
				const Json::Value& roles = opener[1];
				std::vector<CCircuitDef::RoleType> queue;
				queue.reserve(roles.size());
				for (const Json::Value& role : roles) {
					auto it = roleNames.find(role.asString());
					if (it == roleNames.end()) {
						circuit->LOG("CONFIG %s: %s start has unknown role '%s'", cfgName.c_str(), defName.c_str(), role.asCString());
					} else {
						queue.push_back(it->second);
					}
				}
				facOpeners.emplace_back(prob, queue);
			}
		}

		const Json::Value& mrph = comm["upgrade"];
		SCommInfo& commInfo = commInfos[commName];
		SCommInfo::SMorph& morph = commInfo.morph;
		morph.frame = mrph.get("time", -1).asInt() * FRAMES_PER_SEC;

		const Json::Value& upgr = mrph["module"];
		morph.modules.reserve(upgr.size());
		for (const Json::Value& lvl : upgr) {
			std::vector<float> mdls;
			for (const Json::Value& m : lvl) {
				mdls.push_back(m.asFloat());
			}
			morph.modules.push_back(mdls);
		}

		const Json::Value& hhdd = comm["hide"];
		SCommInfo::SHide& hide = commInfo.hide;
		hide.frame = hhdd.get("time", -1).asInt() * FRAMES_PER_SEC;
		hide.threat = hhdd.get("threat", 0.f).asFloat();
		hide.isAir = hhdd.get("air", false).asBool();
	}

	if (!commChoices.empty()) {
		unsigned choice = 0;
		float dice = (float)rand() / RAND_MAX * magnitude;
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

bool CSetupManager::HasModules(const CCircuitDef* cdef, unsigned level) const
{
	std::string name = cdef->GetUnitDef()->GetName();
	for (auto& kv : commInfos) {
		if (name.find(kv.first) != std::string::npos) {
			return kv.second.morph.modules.size() > level;
		}
	}
	return false;
}

const std::vector<float>& CSetupManager::GetModules(const CCircuitDef* cdef, unsigned level) const
{
	std::string name = cdef->GetUnitDef()->GetName();
	for (auto& kv : commInfos) {
		if (name.find(kv.first) != std::string::npos) {
			const std::vector<std::vector<float>>& modules = kv.second.morph.modules;
			return modules[std::min<unsigned>(level, modules.size() - 1)];
		}
	}
	const std::vector<std::vector<float>>& modules = commInfos.begin()->second.morph.modules;
	return modules[std::min<unsigned>(level, modules.size() - 1)];
}

int CSetupManager::GetMorphFrame(const CCircuitDef* cdef) const
{
	std::string name = cdef->GetUnitDef()->GetName();
	for (auto& kv : commInfos) {
		if (name.find(kv.first) != std::string::npos) {
			return kv.second.morph.frame;
		}
	}
	return -1;
}

const std::vector<CCircuitDef::RoleType>* CSetupManager::GetOpener(const CCircuitDef* facDef) const
{
	auto its = start.find(commChoice->GetId());
	if (its == start.end()) {
		return nullptr;
	}

	auto ito = its->second.openers.find(facDef->GetId());
	if (ito == its->second.openers.end()) {
		return &its->second.defaultStart;
	}

	float magnitude = 0.f;
	for (const SOpener& opener : ito->second) {
		magnitude += opener.prob;
	}
	float dice = (float)rand() / RAND_MAX * magnitude;
	float total = .0f;
	for (const SOpener& opener : ito->second) {
		total += opener.prob;
		if (dice < total) {
			return &opener.queue;
		}
	}
	return &its->second.defaultStart;
}

const CSetupManager::SCommInfo::SHide* CSetupManager::GetHide(const CCircuitDef* cdef) const
{
	std::string name = cdef->GetUnitDef()->GetName();
	for (auto& kv : commInfos) {
		if (name.find(kv.first) != std::string::npos) {
			return &kv.second.hide;
		}
	}
	return nullptr;
}

void CSetupManager::Welcome() const
{
#ifdef DEBUG_LOG
	Info* info = circuit->GetSkirmishAI()->GetInfo();
	const char* name = info->GetValueByKey("name");
//	OptionValues* options = circuit->GetSkirmishAI()->GetOptionValues();
//	const char* value = options->GetValueByKey("version");
//	const char* version = (value != nullptr) ? value : info->GetValueByKey("version");
	delete info;
//	delete options;

	const int id = circuit->GetSkirmishAIId();
	std::string welcome("/say "/*"a:"*/);

	welcome += std::string(name) + " " + std::string(version) +
			utils::int_to_string(id, " (%i)  Good fun, have luck!");
	circuit->GetGame()->SendTextMessage(welcome.c_str(), 0);
#endif
}

void CSetupManager::FindStart()
{
	if (utils::is_valid(startPos)) {
		circuit->GetScheduler()->RemoveTask(findStart);
		findStart = nullptr;

		for (StartFunc& func : startFuncs) {
			func(startPos);
		}
		return;
	}

	if (circuit->GetTeamUnits().empty()) {
		return;
	}

	int frame = circuit->GetLastFrame();
	AIFloat3 midPos = ZeroVector;
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		midPos += unit->GetPos(frame);
	}
	midPos /= circuit->GetTeamUnits().size();

	float minSqDist = std::numeric_limits<float>::max();
	AIFloat3 bestPos;
	for (auto& kv : circuit->GetTeamUnits()) {
		CCircuitUnit* unit = kv.second;
		const AIFloat3& pos = unit->GetPos(frame);
		float sqDist = pos.SqDistance2D(midPos);
		if (minSqDist > sqDist) {
			minSqDist = sqDist;
			bestPos = pos;
		}
	}
	SetStartPos(bestPos);
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

bool CSetupManager::LoadConfig(const std::string& cfgName)
{
	Info* info = circuit->GetSkirmishAI()->GetInfo();
//	const char* version = info->GetValueByKey("version");
	const char* name = info->GetValueByKey("shortName");
	delete info;

	std::string filename;
	std::string cfgDefault;
	const char* cfgJson;

	if (cfgName.empty()) {
		/*
		 * Try startscript specific config
		 */
		configName = "startscript";
		OptionValues* options = circuit->GetSkirmishAI()->GetOptionValues();
		const char* value = options->GetValueByKey("JSON");
		std::string strJson = ((value != nullptr) && strlen(value) > 0) ? value : "";
		delete options;
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
		filename = std::string("LuaRules/Configs/") + name + "/" + version + "/";
		configName = utils::MakeFileSystemCompatible(map->GetName()) + ".json";
		filename += configName;

		cfgJson = ReadConfig(filename);
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
		filename = std::string("LuaRules/Configs/") + name + "/" + version + "/Default/";
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
	filename = "config" SLASH;
	configName = (cfgDefault.find(".json") == std::string::npos) ? (cfgDefault + ".json") : cfgDefault;
	filename += configName;
	if (LocatePath(filename)) {
		cfgJson = ReadConfig(filename);
		if (cfgJson != nullptr) {
			config = ParseConfig(cfgJson);
			delete[] cfgJson;

			if (config != nullptr) {
				return true;
			}
		}
	} else {
		circuit->LOG("Config file is missing! (%s)", configName.c_str());
	}

	/*
	 * Locate develop config: to run ./spring from source dir
	 */
	filename = std::string("AI/Skirmish/") + name + "/data/config/" + configName;
	cfgJson = ReadConfig(filename);
	if (cfgJson == nullptr){
		return false;
	}

	config = ParseConfig(cfgJson);
	delete[] cfgJson;

	return (config != nullptr);
}

const char* CSetupManager::ReadConfig(const std::string& filename)
{
	File* file = circuit->GetCallback()->GetFile();
	int fileSize = file->GetSize(filename.c_str());
	if (fileSize <= 0) {
		circuit->LOG("No config file! (%s)", filename.c_str());
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
	Json::Reader json;
	Json::Value jsonAll;
	if (!json.parse(cfgJson, jsonAll, false)) {
		circuit->LOG("Malformed config format! (%s)\n%s", configName.c_str(), json.getFormattedErrorMessages().c_str());
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

void CSetupManager::OverrideConfig()
{
	Json::Reader json;
	Json::Value jsonSection;
	OptionValues* options = circuit->GetSkirmishAI()->GetOptionValues();

	const char* value = options->GetValueByKey("factory");
	if ((value != nullptr) && json.parse(value, jsonSection, false)) {
		(*config)["factory"] = jsonSection;
	}

	value = options->GetValueByKey("behaviour");
	if ((value != nullptr) && json.parse(value, jsonSection, false)) {
		(*config)["behaviour"] = jsonSection;
	}

	delete options;
}

} // namespace circuit
