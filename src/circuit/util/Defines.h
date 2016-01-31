/*
 * Defines.h
 *
 *  Created on: Sep 18, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_DEFINES_H_
#define SRC_CIRCUIT_UTIL_DEFINES_H_

#include "Sim/Misc/GlobalConstants.h"
#include "AIFloat3.h"

#include <vector>

#define SQUARE(x)			((x) * (x))
#define SQRT_2				1.4142f
#define FRAMES_PER_SEC		GAME_SPEED
#define WATCHDOG_COUNT		3
#define PYLON_RANGE			500.0f
#define TASK_RETRIES		10
#define DEBUG_MARK			0xBAD0C0DE
#define UNKNOWN_CATEGORY	0xFFFFFFFF

// z++
#define UNIT_FACING_SOUTH	0
// x++
#define UNIT_FACING_EAST	1
// z--
#define UNIT_FACING_NORTH	2
// x--
#define UNIT_FACING_WEST	3

#define MIN_BUILD_SEC	16
#define MAX_BUILD_SEC	40
#define MAX_TRAVEL_SEC	60
#define ASSIGN_TIMEOUT	(FRAMES_PER_SEC * 300)

#define THREAT_BASE		1.0f
#define THREAT_MOD		(1.0f / 20.0f)
#define THREAT_MIN		2.0f
#define THREAT_RES		8
#define DEFAULT_SLACK	(SQUARE_SIZE * THREAT_RES)

#define MAX_SPEED		1e6f
typedef std::vector<springai::AIFloat3> F3Vec;

#endif // SRC_CIRCUIT_UTIL_DEFINES_H_
