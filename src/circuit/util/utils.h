/*
 * utils.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_UTILS_H_
#define SRC_CIRCUIT_UTIL_UTILS_H_

#include "Sim/Misc/GlobalConstants.h"

#include <chrono>
#include <thread>

#include <string.h>
#include <stdarg.h>  // for va_start, etc
#include <memory>    // for std::unique_ptr
//#include <random>
//#include <iterator>

// debug
#ifdef DEBUG_VIS
	#include "DebugDrawer.h"
#endif
#include "Map.h"  // to get Drawer
#include "Drawer.h"
#include "Log.h"
#include "Game.h"  // to pause

namespace utils {

#ifdef DEBUG
	#define PRINT_DEBUG(fmt, ...)	printf((std::string("<CircuitAI DEBUG> ") + utils::string_format(std::string(fmt), ##__VA_ARGS__)).c_str())
#else
	#define PRINT_DEBUG(fmt, ...)
#endif

#define FRAMES_PER_SEC		GAME_SPEED
#define WATCHDOG_COUNT		2
#define PYLON_RANGE			500.0f
#define TASK_RETRIES		10

// z++
#define UNIT_FACING_SOUTH	0
// x++
#define UNIT_FACING_EAST	1
// z--
#define UNIT_FACING_NORTH	2
// x--
#define UNIT_FACING_WEST	3

//template<typename Iter, typename RandomGenerator>
//Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
//	std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
//	std::advance(start, dis(g));
//	return start;
//}
//
//template<typename Iter>
//Iter select_randomly(Iter start, Iter end) {
//	static std::random_device rd;
////	static std::mt19937 gen(rd());
//	static std::default_random_engine gen(rd());
//	return select_randomly(start, end, gen);
//}

template <class C> void free_clear(C& cntr)
{
	for (typename C::iterator it = cntr.begin(); it != cntr.end(); ++it) {
		delete *it;
	}
	cntr.clear();
}

static std::string string_format(const std::string fmt_str, ...)
{
    int final_n, n = ((int)fmt_str.size()) * 2; /* reserve 2 times as much as the length of the fmt_str */
    std::string str;
    std::unique_ptr<char[]> formatted;
    va_list ap;
    while(1) {
        formatted.reset(new char[n]); /* wrap the plain char array into the unique_ptr */
        strcpy(&formatted[0], fmt_str.c_str());
        va_start(ap, fmt_str);
        final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
        va_end(ap);
        if (final_n < 0 || final_n >= n)
            n += abs(final_n - n + 1);
        else
            break;
    }
    return std::string(formatted.get());
}

static inline std::string int_to_string(int i, const std::string &format = "%i")
{
	char buf[64];
	snprintf(buf, sizeof(buf), format.c_str(), i);
	return std::string(buf);
}

static int string_to_int(const std::string &str)
{
    try {
        std::size_t lastChar;
        int result = std::stoi(str, &lastChar, 10);
        if (lastChar != str.size()) {
        	return 0;
        }
        return result;
    }
    catch (std::invalid_argument &) {
        return 0;
    }
    catch (std::out_of_range &) {
        return 0;
    }
}

static inline std::string float_to_string(float f, const std::string &format = "%f")
{
	char buf[64];
	snprintf(buf, sizeof(buf), format.c_str(), f);
	return std::string(buf);
}

static float string_to_float(const std::string &str)
{
    try {
        std::size_t lastChar;
        float result = std::stof(str, &lastChar);
        if (lastChar != str.size()) {
        	return 0.0f;
        }
        return result;
    }
    catch (...) {
        return 0;
    }
}

static inline void sleep(int64_t seconds)
{
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

//template<typename T>
//static inline void file_read(T* value, FILE* file)
//{
//	const size_t readCount = fread(value, sizeof(T), 1, file);
//	if (readCount != 1) {
//		throw std::runtime_error("failed reading from file");
//	}
//}
//
//template<typename T>
//static inline void file_write(const T* value, FILE* file)
//{
//	const size_t writeCount = fwrite(value, sizeof(T), 1, file);
//	if (writeCount != 1) {
//		throw std::runtime_error("failed writing to file");
//	}
//}
//
//static bool IsFSGoodChar(const char c)
//{
//	if ((c >= '0') && (c <= '9')) {
//		return true;
//	} else if ((c >= 'a') && (c <= 'z')) {
//		return true;
//	} else if ((c >= 'A') && (c <= 'Z')) {
//		return true;
//	} else if ((c == '.') || (c == '_') || (c == '-')) {
//		return true;
//	}
//
//	return false;
//}
//
//static std::string MakeFileSystemCompatible(const std::string& str)
//{
//	std::string cleaned = str;
//
//	for (std::string::size_type i=0; i < cleaned.size(); i++) {
//		if (!IsFSGoodChar(cleaned[i])) {
//			cleaned[i] = '_';
//		}
//	}
//
//	return cleaned;
//}

///*
// *  Check if projected point onto line lies between 2 end-points
// */
//auto isInBounds = [&P0, &P](const AIFloat3& P1) {
//	float dx = P1.x - P0.x;
//	float dz = P1.z - P0.z;
//	float mult = ((P.x - P0.x) * dx + (P.z - P0.z) * dz) / (dx * dx + dz * dz);
//
//	float projX = P0.x + dx * mult;
//	float projZ = P0.z + dz * mult;
//	return ((std::min(P0.x, P1.x) <= projX) && (projX <= std::max(P0.x, P1.x)) &&
//			(std::min(P0.z, P1.z) <= projZ) && (projZ <= std::max(P0.z, P1.z)));
//};

///*
// *  Distance from line to point, squared
// */
//auto sqDistPointToLine = [&P0, &P](const AIFloat3& P1) {
//	float A = P0.z - P1.z;
//	float B = P1.x - P0.x;
//	float C = P0.x * P1.z - P1.x * P0.z;
//	float denominator = A * P.x + B * P.z + C;
//	float numerator = A * A + B * B;
//	return denominator * denominator / numerator;
//};

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_UTILS_H_
