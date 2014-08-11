/*
 * utils.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef UTILS_H_
#define UTILS_H_

#include <chrono>
#include <thread>

#include <string.h>
#include <stdarg.h>  // for va_start, etc
#include <memory>    // for std::unique_ptr

namespace utils {

#define SQUARE_SIZE		8

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
	std::this_thread::sleep_for(std::chrono::milliseconds(seconds * 1000));
}

} // namespace utils

#endif // UTILS_H_
