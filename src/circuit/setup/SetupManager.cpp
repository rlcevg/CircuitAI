/*
 * SetupManager.cpp
 *
 *  Created on: Dec 9, 2014
 *      Author: rlcevg
 */

#include "setup/SetupManager.h"
#include "setup/SetupData.h"
#include "module/MilitaryManager.h"  // only for CalcLanePos
#include "resource/MetalManager.h"
#include "scheduler/Scheduler.h"
#include "terrain/TerrainManager.h"
#include "CircuitAI.h"
#include "util/GameAttribute.h"
#include "util/FileSystem.h"
#include "util/Utils.h"
#include "json/json.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "OptionValues.h"
#include "SkirmishAI.h"
#include "Info.h"
#include "Game.h"
#include "Log.h"
#include "Lua.h"

#include <regex>

namespace circuit {

using namespace springai;
using namespace terrain;

CSetupManager::CSetupManager(CCircuitAI* circuit, CSetupData* setupData)
		: circuit(circuit)
		, setupData(setupData)
		, config(nullptr)
		, commander(nullptr)
		, startPos(-RgtVector)
		, basePos(-RgtVector)
		, lanePos(-RgtVector)
		, metalBase(-RgtVector)
		, energyBase(-RgtVector)
		, energyBase2(-RgtVector)
		, emptyShield(0.f)
		, commChoice(nullptr)
		, isSideSelected(false)
{
	const char* setupScript = circuit->GetGame()->GetSetupScript();
	if (!setupData->IsInitialized()) {
		setupData->ParseSetupScript(circuit, setupScript);
	}
	DisabledUnits(setupScript);

	findStart = CScheduler::GameJob(&CSetupManager::FindStart, this);
	circuit->GetScheduler()->RunJobEvery(findStart, 1);
}

CSetupManager::~CSetupManager()
{
	delete config;
}

void CSetupManager::DisabledUnits(const char* setupScript)
{
	std::string script(setupScript);
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

	std::string modoptionsTag("[modoptions]");
	std::string::const_iterator bodyBegin = std::search(
			script.begin(), script.end(),
			modoptionsTag.begin(), modoptionsTag.end(),
			[](char ch1, char ch2) { return std::tolower(ch1) == ch2; }
	);
	if (bodyBegin != script.end()) {
		std::advance(bodyBegin, modoptionsTag.length());
		std::string::const_iterator bodyEnd = utils::EndInBraces(bodyBegin, script.end());

		std::smatch disabledunits;
		std::regex patternDisabled("disabledunits=(.*);", std::regex::ECMAScript | std::regex::icase);
		if (std::regex_search(bodyBegin, bodyEnd, disabledunits, patternDisabled)) {
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

bool CSetupManager::OpenConfig(const std::string& profile, const std::vector<std::string>& parts)
{
	bool isOk = LoadConfig(profile, parts);
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
	CAllyTeam* allyTeam = circuit->GetAllyTeam();
	const utils::CRegion& box = allyTeam->GetStartBox();
	AIFloat3 pos;

	switch (type) {
		case StartPosType::METAL_SPOT: {
			const CMetalData::Clusters& clusters = circuit->GetMetalManager()->GetClusters();
			const CMetalData::Metals& spots = circuit->GetMetalManager()->GetSpots();
			CTerrainManager* terrainMgr = circuit->GetTerrainManager();
			SMobileType* mobileType = terrainMgr->GetMobileTypeById(commChoice->GetMobileId());

			std::map<int, CMetalData::MetalIndices> validPoints;
			for (unsigned idx = 0; idx < clusters.size(); ++idx) {
				for (int i : clusters[idx].idxSpots) {
					// ZK has "ai_is_valid_startpos:123.12/321.32" command, @see LuaRules/Gadgets/start_boxes.lua
					if (!box.ContainsPoint(spots[i].position)) {
						continue;
					}

					const int iS = terrainMgr->GetSectorIndex(spots[i].position);
					SArea* area = mobileType->sector[iS].area;
					if ((area != nullptr) && area->areaUsable) {
						validPoints[idx].push_back(i);
					}
				}
			}

			if (!validPoints.empty()) {
				struct SCluster {
					unsigned count;
					float distDivIncome;
				};
				const AIFloat3 mapCenter = terrainMgr->GetTerrainCenter();
				std::vector<std::pair<int, SCluster>> validClusters;
				for (auto& kv : validPoints) {
					SCluster c;
					c.count = allyTeam->GetClusterTeam(kv.first).count;
					const CMetalData::SCluster& cl = clusters[kv.first];
					const float income = cl.income + (float)rand() / RAND_MAX - 0.5f;
					c.distDivIncome = mapCenter.distance(cl.position) / income;
					validClusters.push_back(std::make_pair(kv.first, c));
				}
				std::random_shuffle(validClusters.begin(), validClusters.end());

				auto cmp = [](const std::pair<int, SCluster>& a, const std::pair<int, SCluster>& b) {
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
				pos = spots[indices[rand() % indices.size()]].position;
			} else {
				pos = box.Random();
			}
		} break;
		case StartPosType::RANDOM:
		default: {
			pos = box.Random();
		} break;
	}

	pos.y = circuit->GetMap()->GetElevationAt(pos.x, pos.z);
	SetBasePos(pos);
	circuit->GetGame()->SendStartPosition(false, pos);
}

void CSetupManager::SetBasePos(const AIFloat3& pos)
{
	basePos = pos;
	const AIFloat3 mapCenter = circuit->GetTerrainManager()->GetTerrainCenter();
	AIFloat3 refPos;
//	if (circuit->GetMetalManager()->GetClusters().empty()) {
		refPos = pos;
//	} else {
//		int index = circuit->GetMetalManager()->FindNearestCluster(pos);
//		refPos = circuit->GetMetalManager()->GetClusters()[index].position;
//	}
	const AIFloat3 enemyDir = (mapCenter - refPos).Normalize2D();
	const AIFloat3 normal = AIFloat3(-enemyDir.z, 0, enemyDir.x) * ((rand() < RAND_MAX / 2) ? -1.f : 1.f);
	const AIFloat3 backPos = refPos - enemyDir * 400.f;
	metalBase = backPos + normal * 200.f;
	energyBase = backPos - normal * 200.f;
	energyBase2 = backPos - enemyDir * 400.f;
	CTerrainManager::CorrectPosition(metalBase);
	CTerrainManager::CorrectPosition(energyBase);
	CTerrainManager::CorrectPosition(energyBase2);
}

bool CSetupManager::PickCommander()
{
	std::vector<CCircuitDef*> comms;
	float bestPower = .0f;

	if (commChoice == nullptr) {
		for (CCircuitDef& cdef : circuit->GetCircuitDefs()) {
			std::string lvl1 = cdef.GetDef()->GetName();
			if ((lvl1.find(commPrefix) != 0) || (lvl1.find(commSuffix) != lvl1.size() - 5)) {
				continue;
			}

			const std::map<std::string, std::string>& customParams = cdef.GetDef()->GetCustomParams();
			auto it = customParams.find("level");
			if ((it == customParams.end()) || (utils::string_to_int(it->second) != 1)) {
				continue;
			}
			comms.push_back(&cdef);

			if (bestPower < cdef.GetBuildDistance()) {  // No more UnitDef->GetAutoHeal() :(
				bestPower = cdef.GetBuildDistance();
				commChoice = &cdef;
			}
		}
		if (comms.empty()) {
			return false;
		}
	}

	std::string cmd("ai_commander:");
	cmd += ((commChoice == nullptr) ? comms[rand() % comms.size()] : commChoice)->GetDef()->GetName();
	circuit->GetLua()->CallRules(cmd.c_str(), cmd.size());

	return true;
}

void CSetupManager::SetCommander(CCircuitUnit* unit)
{
	commander = unit;
	if (unit == nullptr) {
		return;
	}
	commChoice = commander->GetCircuitDef();
	if (!utils::is_valid(basePos)) {
		SetBasePos(unit->GetPos(circuit->GetLastFrame()));
	}

	if (isSideSelected) {
		return;
	}
	auto it = sides.find(unit->GetCircuitDef()->GetId());
	if (it != sides.end()) {
		circuit->SetSide(it->second);
		isSideSelected = true;
	}
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
	commPrefix = comm["prefix"].asString();
	commSuffix = comm["suffix"].asString();
//	std::vector<CCircuitDef*> commChoices;
//	commChoices.reserve(items.size());
	float magnitude = 0.f;
	std::vector<float> weight;
	weight.reserve(items.size());
	CCircuitDef::RoleName& roleNames = CCircuitDef::GetRoleNames();

	for (const std::string& commName : items.getMemberNames()) {
		CCircuitDef* cdef = circuit->GetCircuitDef((commPrefix + commName + commSuffix).c_str());
		if (cdef == nullptr) {
			circuit->LOG("CONFIG %s: has unknown commander '%s'", cfgName.c_str(), commName.c_str());
			continue;
		}

//		commChoices.push_back(cdef);

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
				facStart.defaultStart.push_back(it->second.type);
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
				std::vector<CCircuitDef::RoleT> queue;
				queue.reserve(roles.size());
				for (const Json::Value& role : roles) {
					auto it = roleNames.find(role.asString());
					if (it == roleNames.end()) {
						circuit->LOG("CONFIG %s: %s start has unknown role '%s'", cfgName.c_str(), defName.c_str(), role.asCString());
					} else {
						queue.push_back(it->second.type);
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
		const Json::Value& taskRad = hhdd["task_rad"];
		hide.sqPeaceTaskRad = std::min(taskRad.get((unsigned)0, 2000.f).asFloat(), CTerrainManager::GetTerrainDiagonal() * 0.5f);
		if (hide.sqPeaceTaskRad > 0.f) {
			hide.sqPeaceTaskRad = SQUARE(hide.sqPeaceTaskRad);
		}
		hide.sqDangerTaskRad = SQUARE(taskRad.get((unsigned)1, 1000.f).asFloat());

		CCircuitDef* commDef = circuit->GetCircuitDef(commName.c_str());
		const std::string& commSide = comm.get("side", "").asString();
		sides[commDef->GetId()] = commSide;
		if (circuit->GetSideName() == commSide) {
			commChoice = commDef;
		}
	}

//	if (!commChoices.empty()) {
//		unsigned choice = 0;
//		float dice = (float)rand() / RAND_MAX * magnitude;
//		for (unsigned i = 0; i < weight.size(); ++i) {
//			dice -= weight[i];
//			if (dice < 0.f) {
//				choice = i;
//				break;
//			}
//		}
//		commChoice = commChoices[choice];
//	}
}

bool CSetupManager::HasModules(const CCircuitDef* cdef, unsigned level) const
{
	std::string name = cdef->GetDef()->GetName();
	for (auto& kv : commInfos) {
		if (name.find(kv.first) != std::string::npos) {
			return kv.second.morph.modules.size() > level;
		}
	}
	return false;
}

const std::vector<float>& CSetupManager::GetModules(const CCircuitDef* cdef, unsigned level) const
{
	std::string name = cdef->GetDef()->GetName();
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
	std::string name = cdef->GetDef()->GetName();
	for (auto& kv : commInfos) {
		if (name.find(kv.first) != std::string::npos) {
			return kv.second.morph.frame;
		}
	}
	return -1;
}

const std::vector<CCircuitDef::RoleT>* CSetupManager::GetOpener(const CCircuitDef* facDef) const
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
	for (const SOpener& opener : ito->second) {
		dice -= opener.prob;
		if (dice < 0.f) {
			return &opener.queue;
		}
	}
	return &its->second.defaultStart;
}

const CSetupManager::SCommInfo::SHide* CSetupManager::GetHide(const CCircuitDef* cdef) const
{
	std::string name = cdef->GetDef()->GetName();
	for (auto& kv : commInfos) {
		if (name.find(kv.first) != std::string::npos) {
			return &kv.second.hide;
		}
	}
	return nullptr;
}

void CSetupManager::Welcome() const
{
	Info* info = circuit->GetSkirmishAI()->GetInfo();
	const char* name = info->GetValueByKey("name");
//	OptionValues* options = circuit->GetSkirmishAI()->GetOptionValues();
//	const char* value = options->GetValueByKey("version");
//	const char* version = (value != nullptr) ? value : info->GetValueByKey("version");
	delete info;
//	delete options;

	const int id = circuit->GetSkirmishAIId();
#ifdef DEBUG_LOG
	std::string welcome("/say "/*"a:"*/);
	welcome += std::string(name) + " " + std::string(version) +
			utils::int_to_string(id, " (%i)  Good fun, have luck!");
	circuit->GetGame()->SendTextMessage(welcome.c_str(), 0);
#else
	std::string welcome = std::string(name) + " " + std::string(version) +
			utils::int_to_string(id, " (%i) Initialized!");
	circuit->LOG("%s", welcome.c_str());
#endif
}

void CSetupManager::FindStart()
{
	if (utils::is_valid(startPos)) {
		circuit->GetScheduler()->RemoveJob(findStart);
		findStart = nullptr;

		for (StartFunc& func : startFuncs) {
			func(startPos);
		}

		circuit->GetScheduler()->RunJobAfter(CScheduler::GameJob(&CSetupManager::CalcLanePos, this), FRAMES_PER_SEC);
		return;
	}

	if (circuit->GetTeamUnits().empty()) {
		return;
	}

	CalcStartPos();
}

void CSetupManager::CalcStartPos()
{
	const int frame = circuit->GetLastFrame();
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

void CSetupManager::CalcLanePos()
{
	std::vector<std::pair<int, AIFloat3>> points;
	AIFloat3 midPos = ZeroVector;
	for (CCircuitAI* ai : circuit->GetGameAttribute()->GetCircuits()) {
		if (ai->IsInitialized() && (ai->GetAllyTeamId() == circuit->GetAllyTeamId())) {
			const AIFloat3& pos = ai->GetSetupManager()->GetBasePos();
			points.push_back(std::make_pair(ai->GetSkirmishAIId(), pos));
			midPos += pos;
		}
	}
	midPos /= points.size();

	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	float width = terrainMgr->GetTerrainWidth();
	float height = terrainMgr->GetTerrainHeight();
	AIFloat3 p1(width - width / 3, 0, height / 3);
	AIFloat3 p2(width / 3, 0, height / 3);
	AIFloat3 p3(width / 3, 0, height - height / 3);
	AIFloat3 p4(width - width / 3, 0, height - height / 3);
	AIFloat3 centerPos(width / 2, 0, height / 2);
	AIFloat3 step, offset;
	if ((midPos.x > p1.x && midPos.z < p1.z) || (midPos.x < p3.x && midPos.z > p3.z)) {  // I, III
		step = AIFloat3(width / (points.size() + 1), 0, height / (points.size() + 1));
		offset = AIFloat3(0, 0, 0);
	} else if ((midPos.x < p2.x && midPos.z < p2.z) || (midPos.x > p4.x && midPos.z > p4.z)) {  // II, IV
		step = AIFloat3(width / (points.size() + 1), 0, -height / (points.size() + 1));
		offset = AIFloat3(0, 0, height);
	} else if (std::fabs(centerPos.x - midPos.x) * height < std::fabs(centerPos.z - midPos.z) * width) {  // x-aligned (dX/dZ < w/h)
		step = AIFloat3(width / (points.size() + 1), 0, 0);
		offset = AIFloat3(0, 0, height / 2);
	} else {  // z-aligned
		step = AIFloat3(0, 0, height / (points.size() + 1));
		offset = AIFloat3(width / 2, 0, 0);
	}

	auto sqLenOfProjPointOnLine = [](const AIFloat3& p/*, const AIFloat3& v1*/, const AIFloat3& v2) {
		// simplified, v1 = ZeroPoint
		float e1x = v2.x/* - v1.x*/, e1z = v2.z/* - v1.z*/;  // v2-ZeroPoint
		float e2x = p.x/* - v1.x*/, e2z = p.z/* - v1.z*/;  // p-ZeroPoint
		float valDp = e1x * e2x + e1z * e2z;  // dotProduct(e1, e2)
		float sqLen = e1x * e1x + e1z * e1z;
		float resX = /*v1.x + */(valDp * e1x) / sqLen;
		float resZ = /*v1.z + */(valDp * e1z) / sqLen;
		return resX * resX + resZ * resZ;
	};
	std::vector<std::pair<int, float>> sortedPoints;
	for (const std::pair<int, AIFloat3>& pos : points) {
		sortedPoints.push_back(std::make_pair(pos.first, sqLenOfProjPointOnLine(pos.second, step)));
	}
	auto compare = [](const std::pair<int, float>& p1, const std::pair<int, float>& p2) {
		return p1.second < p2.second;
	};
	std::sort(sortedPoints.begin(), sortedPoints.end(), compare);
	int i = 0;
	while (i < (int)sortedPoints.size() && circuit->GetSkirmishAIId() != sortedPoints[i].first) {
		i++;
	}
	AIFloat3 bestPos = step * (i + 1) + offset;

	SetLanePos(bestPos);
#ifdef DEBUG_VIS
	circuit->GetDrawer()->AddPoint((basePos + lanePos) / 2, utils::int_to_string(circuit->GetTeamId()).c_str());
	circuit->GetDrawer()->AddLine((basePos + lanePos) / 2, lanePos);
	circuit->LOG("baseRange: %f", lanePos.distance2D(basePos));
#endif  // DEBUG_VIS

	// NOTE: #include "module/MilitaryManager.h"
	circuit->GetMilitaryManager()->SetBaseDefRange(lanePos.distance2D(basePos));

	// FIXME: Influence map gets broken
//	const AIFloat3 mapCenter = CTerrainManager::GetTerrainCenter();
//	CAllyTeam* allyTeam = circuit->GetAllyTeam();
//	if (mapCenter.SqDistance2D(lanePos) < mapCenter.SqDistance2D(allyTeam->GetAuthority()->GetSetupManager()->GetLanePos())) {
//		allyTeam->SetAuthority(circuit);
//	}
}

bool CSetupManager::LoadConfig(const std::string& profile, const std::vector<std::string>& parts)
{
	configName = profile;
	std::string dirname;

	/*
	 * Locate game-side config
	 */
	OptionValues* options = circuit->GetSkirmishAI()->GetOptionValues();
	const char* value = options->GetValueByKey("game_config");
	delete options;
	if ((value != nullptr) && StringToBool(value)) {
		dirname = utils::GetAIDataGameDir(circuit->GetSkirmishAI(), "config");
		config = ReadConfig(dirname, profile, parts, true);
		if (config != nullptr) {
			return true;
		} else {
			circuit->LOG("Game-side config: '%s' is missing!", (dirname + profile + SLASH).c_str());
		}
	}

	/*
	 * Locate AI config
	 */
	dirname = "config" SLASH;
	if (utils::LocatePath(circuit->GetCallback(), dirname)) {
		config = ReadConfig(dirname, profile, parts, false);
	} else {
		circuit->LOG("AI config: '%s' is missing!", (dirname + profile + SLASH).c_str());
	}
	return (config != nullptr);
}

Json::Value* CSetupManager::ReadConfig(const std::string& dirname, const std::string& profile,
		const std::vector<std::string>& parts, const bool isVFS)
{
	Json::Value* cfg = nullptr;
	bool isForceLoad = !isVFS;
	if (!isForceLoad) {
		for (const std::string& name : parts) {
			std::string filename = dirname + profile + SLASH + name + ".json";
			if (utils::FileExists(circuit->GetCallback(), filename)) {
				isForceLoad = true;
				break;
			}
		}
	}
	if (isForceLoad) {
		for (const std::string& name : parts) {
			std::string filename = dirname + profile + SLASH + name + ".json";
			auto cfgStr = utils::ReadFile(circuit->GetCallback(), filename);
			if (cfgStr.empty()) {
				filename = dirname + name + ".json";
				cfgStr = utils::ReadFile(circuit->GetCallback(), filename);
				if (cfgStr.empty()) {
					circuit->LOG("No config file! (%s)", filename.c_str());
					continue;
				}
			}
			circuit->LOG("Load config: %s", filename.c_str());
			cfg = ParseConfig(cfgStr, name, cfg);
		}
	}
	return cfg;
}

Json::Value* CSetupManager::ParseConfig(const std::string& cfgStr, const std::string& cfgName, Json::Value* cfg)
{
	Json::CharReader* reader = Json::CharReaderBuilder().newCharReader();
	JSONCPP_STRING errs;
	Json::Value json;
	bool ok = reader->parse(cfgStr.c_str(), cfgStr.c_str() + cfgStr.size(), &json, &errs);
	delete reader;
	if (!ok) {
		circuit->LOG("Malformed config format! (%s)\n%s", cfgName.c_str(), errs.c_str());
		return nullptr;
	}

	if (cfg == nullptr) {
		cfg = new Json::Value;
		*cfg = json;
	} else {
		UpdateJson(*cfg, json);
	}
	return cfg;
}

void CSetupManager::UpdateJson(Json::Value& a, Json::Value& b) {
	if (!a.isObject() || !b.isObject()) {
		return;
	}

	for (const auto& key : b.getMemberNames()) {
		if (a[key].isObject()) {
			UpdateJson(a[key], b[key]);
		} else {
			if (!a[key].isNull()) {
				// TODO: Make path for key
				circuit->LOG("Config override: %s", key.c_str());
			}
			a[key] = b[key];
		}
	}
}

void CSetupManager::OverrideConfig()
{
	/*
	 * Check startscript specific config
	 */
	OptionValues* options = circuit->GetSkirmishAI()->GetOptionValues();
	const char* value = options->GetValueByKey("JSON");
	std::string cfgStr = ((value != nullptr) && strlen(value) > 0) ? value : "";
	delete options;
	if (!cfgStr.empty()) {
		circuit->LOG("Override config %s by startscript", configName.c_str());
		config = ParseConfig(cfgStr, "startscript", config);
	}
}

} // namespace circuit
