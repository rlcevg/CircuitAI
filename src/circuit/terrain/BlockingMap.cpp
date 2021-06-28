/*
 * BlockingMap.cpp
 *
 *  Created on: Aug 1, 2016
 *      Author: rlcevg
 */

#include "terrain/BlockingMap.h"

namespace circuit {

SBlockingMap::StructTypes SBlockingMap::structTypes = {
	{"factory",   SBlockingMap::StructType::FACTORY},
	{"mex",       SBlockingMap::StructType::MEX},
	{"geo",       SBlockingMap::StructType::GEO},
	{"pylon",     SBlockingMap::StructType::PYLON},
	{"convert",   SBlockingMap::StructType::CONVERT},
	{"engy_low",  SBlockingMap::StructType::ENGY_LOW},
	{"engy_mid",  SBlockingMap::StructType::ENGY_MID},
	{"engy_high", SBlockingMap::StructType::ENGY_HIGH},
	{"def_low",   SBlockingMap::StructType::DEF_LOW},
	{"def_mid",   SBlockingMap::StructType::DEF_MID},
	{"def_high",  SBlockingMap::StructType::DEF_HIGH},
	{"special",   SBlockingMap::StructType::SPECIAL},
	{"nano",      SBlockingMap::StructType::NANO},
	{"terra",     SBlockingMap::StructType::TERRA},
	{"unknown",   SBlockingMap::StructType::UNKNOWN},
};

SBlockingMap::StructMasks SBlockingMap::structMasks = {
	{"factory",   SBlockingMap::StructMask::FACTORY},
	{"mex",       SBlockingMap::StructMask::MEX},
	{"geo",       SBlockingMap::StructMask::GEO},
	{"pylon",     SBlockingMap::StructMask::PYLON},
	{"convert",   SBlockingMap::StructMask::CONVERT},
	{"engy_low",  SBlockingMap::StructMask::ENGY_LOW},
	{"engy_mid",  SBlockingMap::StructMask::ENGY_MID},
	{"engy_high", SBlockingMap::StructMask::ENGY_HIGH},
	{"def_low",   SBlockingMap::StructMask::DEF_LOW},
	{"def_mid",   SBlockingMap::StructMask::DEF_MID},
	{"def_high",  SBlockingMap::StructMask::DEF_HIGH},
	{"special",   SBlockingMap::StructMask::SPECIAL},
	{"nano",      SBlockingMap::StructMask::NANO},
	{"terra",     SBlockingMap::StructMask::TERRA},
	{"unknown",   SBlockingMap::StructMask::UNKNOWN},
	{"none",      SBlockingMap::StructMask::NONE},
	{"all",       SBlockingMap::StructMask::ALL},
};

} // namespace circuit
