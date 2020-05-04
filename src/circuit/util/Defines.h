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
#include <string.h>

#ifdef DEBUG_VIS
#define DEBUG_LOG
#endif

template<typename T> static inline constexpr T SQUARE(T x) { return x * x; }
// sqrt(2) ~= 1.4142f
#define SQRT_2				1.4142f
#define ISQRT_2				(1.f / 1.4142f)
#define FRAMES_PER_SEC		GAME_SPEED
#define WATCHDOG_COUNT		3
#define TASK_RETRIES		10
#define DEBUG_MARK			0xBAD0C0DE
#define UNKNOWN_CATEGORY	0xFFFFFFFF
#define NO_SPEED_LIMIT		-1.0f

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

#define THREAT_MIN		1.0f
#define DEFAULT_SLACK	(SQUARE_SIZE * 8)
#define SHIELD_MOD		1.5f

typedef std::vector<springai::AIFloat3> F3Vec;
typedef std::vector<void*> VoidVec;
typedef std::vector<float> FloatVec;
typedef std::vector<bool> BoolVec;
typedef std::vector<int> IndexVec;
typedef std::vector<int> IntVec;

struct PathInfo {
	PathInfo(bool last = false) : start(0), isLast(last) {}
	void Clear() { posPath.clear(); path.clear(); }  // FIXME: stop TravelAction
	F3Vec posPath;
	IndexVec path;
	size_t start;
	bool isLast;
};

struct cmp_str {
	bool operator()(char const* a, char const* b) {
		return strcmp(a, b) < 0;
	}
};

#endif // SRC_CIRCUIT_UTIL_DEFINES_H_
