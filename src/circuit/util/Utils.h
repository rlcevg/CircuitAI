/*
 * utils.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_UTILS_H_
#define SRC_CIRCUIT_UTIL_UTILS_H_

#include "util/Defines.h"
#include "util/math/Geometry.h"

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
#include "Lua.h"
#endif

#ifdef DEBUG_LOG
#include "CircuitAI.h"

#include "spring/SpringMap.h"  // to get Drawer
#include "Drawer.h"
#include "Log.h"
#include "Game.h"  // to pause
#endif

namespace utils {

#ifdef DEBUG
	#define PRINT_DEBUG(fmt, ...)	printf("<CircuitAI DEBUG> " fmt, ##__VA_ARGS__)
	#define ASSERT(x)	assert(x)
#else
	#define PRINT_DEBUG(fmt, ...)
	#define ASSERT(x)	if (!(x)) throw
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

template <class C> void free(C& cntr)
{
	for (typename C::const_iterator it = cntr.cbegin(); it != cntr.cend(); ++it) {
		delete *it;
	}
}

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

static inline std::string::const_iterator EndInBraces(const std::string::const_iterator begin, const std::string::const_iterator end)
{
	std::string::const_iterator brEnd = begin;
	int openBr = 0;
	for (; brEnd != end; ++brEnd) {
		if (*brEnd == '{') {
			++openBr;
		} else if (*brEnd == '}') {
			if (--openBr == 0) {
				break;
			}
		}
	}
	return brEnd;
}

//auto isInBoundsAndSqDistToLineLessThan = [](const AIFloat3& P0, const AIFloat3& P1, const AIFloat3& P, const float sqDist) {
//	const float dx = P1.x - P0.x;
//	const float dz = P1.z - P0.z;
//	const float innerProduct = (P.x - P0.x) * dx + (P.z - P0.z) * dz;
//	const float numerator = dx * dx + dz * dz;
//	if (0 <= innerProduct && innerProduct <= numerator) {  // isInSegment
//		float C = P1.x * P0.z - P1.z * P0.x;
//		float denominator = dz * P.x - dx * P.z + C;
//		return denominator * denominator / numerator <= sqDist;  // compare square distances
//	}
//	return false;
//};

template<class T> static inline constexpr T clamp(const T v, const T vmin, const T vmax)
{
	return std::min(vmax, std::max(vmin, v));
}

template<typename T> static inline std::ostream& binary_write(std::ostream& stream, const T& value)
{
	return stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T> static inline std::istream& binary_read(std::istream& stream, T& value)
{
	return stream.read(reinterpret_cast<char*>(&value), sizeof(T));
}

#ifdef DEBUG_LOG
	template<typename units>
	class CScopedTime {
	public:
		using clock = std::chrono::high_resolution_clock;
		CScopedTime(circuit::CCircuitAI* ai, const std::string& msg, int t)
			: ai(ai), text(msg), t0(clock::now()), thr(t), callback(nullptr)
		{}
		CScopedTime(circuit::CCircuitAI* ai, const std::string& msg, int t, std::function<void(circuit::CCircuitAI*)> cb)
			: ai(ai), text(msg), t0(clock::now()), thr(t), callback(cb)
		{}
		~CScopedTime() {
			int count = std::chrono::duration_cast<units>(clock::now() - t0).count();
			if (count >= thr) {
				ai->LOG("%i | %i ms | %s", ai->GetSkirmishAIId(), count, text.c_str());
				if (callback != nullptr) callback(ai);
			}
		}
	private:
		circuit::CCircuitAI* ai;
		std::string text;
		clock::time_point t0;
		int thr;
		std::function<void(circuit::CCircuitAI*)> callback;
	};
	#define SCOPED_TIME(x, y) utils::CScopedTime<std::chrono::milliseconds> st(x, y, 10)
	#define SCOPED_TIME_NT(n, x, y, t) utils::CScopedTime<std::chrono::milliseconds> n(x, y, t)
	#define SCOPED_TIME_FN(x, y, t, f) utils::CScopedTime<std::chrono::milliseconds> st(x, y, t, f)
#else
	#define SCOPED_TIME(x, y)
#endif

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_UTILS_H_
