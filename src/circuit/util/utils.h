/*
 * utils.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_UTILS_H_
#define SRC_CIRCUIT_UTIL_UTILS_H_

#include "util/Defines.h"

#include "System/StringUtil.h"
#include "System/Threading/SpringThreading.h"
#include <chrono>

#include <string.h>
#include <stdarg.h>  // for va_start, etc
#include <memory>    // for std::unique_ptr
//#include <random>
//#include <iterator>

#ifdef DEBUG_VIS
#include "DebugDrawer.h"
#endif

#ifdef DEBUG_LOG
#include "Map.h"  // to get Drawer
#include "Drawer.h"
#include "Log.h"
#include "Game.h"  // to pause
#endif

namespace utils {

#ifdef DEBUG
	#define PRINT_DEBUG(fmt, ...)	printf("<CircuitAI DEBUG> " fmt , ##__VA_ARGS__)
#else
	#define PRINT_DEBUG(fmt, ...)
#endif

#ifdef _WIN32
	#define SLASH "\\"
#else
	#define SLASH "/"
#endif

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

static inline std::string string_format(const std::string fmt_str, ...)
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

// FIXME: Use rts/System/StringUtil.h IntToString?
static inline std::string int_to_string(int i, const std::string &format = "%i")
{
	char buf[64];
	snprintf(buf, sizeof(buf), format.c_str(), i);
	return std::string(buf);
}

static inline int string_to_int(const std::string &str, int base = 10)
{
	try {
		std::size_t lastChar;
		int result = std::stoi(str, &lastChar, base);
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

static inline float string_to_float(const std::string &str)
{
	try {
		std::size_t lastChar;
		float result = std::stof(str, &lastChar);
		if (lastChar != str.size()) {
			return 0.f;
		}
		return result;
	}
	catch (...) {
		return 0;
	}
}

static inline void sleep(int64_t seconds)
{
	spring::this_thread::sleep_for(std::chrono::seconds(seconds));
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

static inline bool IsFSGoodChar(const char c)
{
	if ((c >= '0') && (c <= '9')) {
		return true;
	} else if ((c >= 'a') && (c <= 'z')) {
		return true;
	} else if ((c >= 'A') && (c <= 'Z')) {
		return true;
	} else if ((c == '.') || (c == '_') || (c == '-')) {
		return true;
	}

	return false;
}

static inline std::string MakeFileSystemCompatible(const std::string& str)
{
	std::string cleaned = str;

	for (std::string::size_type i=0; i < cleaned.size(); i++) {
		if (!IsFSGoodChar(cleaned[i])) {
			cleaned[i] = '_';
		}
	}

	return cleaned;
}

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

static inline bool is_equal_pos(const springai::AIFloat3& posA, const springai::AIFloat3& posB, const float slack = SQUARE_SIZE * 2)
{
	return (math::fabs(posA.x - posB.x) <= slack) && (math::fabs(posA.z - posB.z) <= slack);
}

static inline springai::AIFloat3 get_near_pos(const springai::AIFloat3& pos, float range)
{
	springai::AIFloat3 offset((float)rand() / RAND_MAX - 0.5f, 0.f, (float)rand() / RAND_MAX - 0.5f);
	return pos + offset * range;
}

static inline springai::AIFloat3 get_radial_pos(const springai::AIFloat3& pos, float radius)
{
	float angle = (float)rand() / RAND_MAX * 2 * M_PI;
	springai::AIFloat3 offset(std::cos(angle), 0.f, std::sin(angle));
	return pos + offset * radius;
}

static inline bool is_valid(const springai::AIFloat3& pos)
{
	return pos.x != -RgtVector.x;
}

template<class T> static inline constexpr T clamp(const T v, const T vmin, const T vmax)
{
	return std::min(vmax, std::max(vmin, v));
}

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_UTILS_H_
