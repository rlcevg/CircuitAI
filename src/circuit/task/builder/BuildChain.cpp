/*
 * BuildChain.cpp
 *
 *  Created on: Aug 11, 2016
 *      Author: rlcevg
 */

#include "task/builder/BuildChain.h"

namespace circuit {

SBuildInfo::DirName SBuildInfo::dirNames = {
	{"left",  SBuildInfo::Direction::LEFT},
	{"right", SBuildInfo::Direction::RIGHT},
	{"front", SBuildInfo::Direction::FRONT},
	{"back",  SBuildInfo::Direction::BACK},
	{"none",  SBuildInfo::Direction::NONE},
};

SBuildInfo::CondName SBuildInfo::condNames = {
	{"air",    SBuildInfo::Condition::AIR},
	{"energy", SBuildInfo::Condition::ENERGY},
	{"wind",   SBuildInfo::Condition::WIND},
	{"sensor", SBuildInfo::Condition::SENSOR},
	{"chance", SBuildInfo::Condition::CHANCE},
	{"always", SBuildInfo::Condition::ALWAYS},
};

SBuildInfo::PrioName SBuildInfo::prioNames = {
	{"low",    IBuilderTask::Priority::LOW},
	{"normal", IBuilderTask::Priority::NORMAL},
	{"high",   IBuilderTask::Priority::HIGH},
	{"now",    IBuilderTask::Priority::NOW},
};

} // namespace circuit
