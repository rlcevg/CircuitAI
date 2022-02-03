/*
	path.h implementation

	Copyright (c) 2015 Wenzel Jakob <wenzel@inf.ethz.ch>

	All rights reserved. Use of this source code is governed by a
	BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <stdexcept>
#include <sstream>
#include <utility>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif
#include <sys/stat.h>

#ifdef __linux
# include <linux/limits.h>
#endif

#if __cplusplus >= 201103L || _MSC_VER >= 1800
#include <tuple>
#endif


NAMESPACE_BEGIN(filesystem)

// Constructors
inline path::path() {
}

inline path::path(const path &path)
	: leafs(path.leafs) {
}

#if __cplusplus >= 201103L || _MSC_VER >= 1800
inline path::path(path &&path)
	: leafs(std::move(path.leafs)) {
}
#endif

inline path::path(const char *string) {
	this->set(string);
}

inline path::path(const std::string &string) {
	this->set(string);
	}

#ifdef _WIN32
inline path::path(const std::wstring &wstring) {
	this->set(wstring);
}

inline path::path(const wchar_t *wstring) {
	this->set(wstring);
}
#endif

inline path &path::operator =(const path &path) {
	leafs = path.leafs;
	return *this;
}

#if __cplusplus >= 201103L || _MSC_VER >= 1800
inline path &path::operator =(path &&path) {
	if (this != &path) {
		leafs = std::move(path.leafs);
	}
	return *this;
}
#endif

#ifdef _WIN32
inline path &path::operator =(const std::wstring &str) {
	this->set(str);
	return *this;
}
#endif


// Observers
inline std::string path::str(char separator) const {
	std::ostringstream oss;

	// write volume
	std::size_t i = 0;
	if (has_volume()) {
		oss << leafs[i];
		++i;
	}

	// write root dir
	if (i < leafs.size() && leafs[i] == "/") {
		oss << separator;
		++i;
	}

	// write first item after root
	if (i < leafs.size()) {
		oss << leafs[i];
		++i;
	}

	// write all other elements preceeded by a separator
	for (; i < leafs.size(); ++i) 
		oss << separator << leafs[i];

	return oss.str();
}

inline std::string path::str() const {
#ifdef _WIN32
	return str('\\');
#else
	return str('/');
#endif
}

inline std::string path::str_generic() const {
	return str('/');
}

#ifdef _WIN32
inline std::wstring path::wstr() const {
	std::string temp = str();
	int size = MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), NULL, 0);
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), &result[0], size);
	return result;
}
#endif

inline std::size_t path::length() const {
	return leafs.size();
}

inline bool path::empty() const {
	return leafs.empty();
}

inline bool path::has_volume() const {
#ifdef _WIN32
	if (!leafs.empty()) {
		const std::string& volume = leafs[0];
		if (volume.size() == 2 && std::isalpha(volume[0]) && volume[1] == ':')
			return true;
	}
#endif
	return false;
}

inline bool path::is_absolute() const {
	if (leafs.empty())
		return false;

#ifdef _WIN32
	if (leafs.size() >= 2 && has_volume() && leafs[1] == "/")
		return true;
#else
	if (leafs[0] == "/")
		return true;
#endif

	return false;
}

inline path path::parent_path() const {
	path result(*this);
	if (!result.leafs.empty())
		result.leafs.pop_back();

	return result;
}

inline path path::dirname() const {
	return parent_path();
}

inline std::string path::filename() const {
	if (leafs.empty())
		return "";
	else {
#ifdef _WIN32
		bool _has_volume = has_volume();
		std::size_t _size = leafs.size();

		if (_has_volume && _size == 2 && leafs[1] == "/")
			return "\\";
		else if (!_has_volume && _size == 1 && leafs[0] == "/")
			return "\\";
		else
			return leafs.back();
#else
		return leafs.back();
#endif
	}
}

inline std::string path::basename() const {
	return filename();
}

inline std::size_t path::extension_pos(const std::string& name) {
	if (name == "." || name == "..")
		return std::string::npos;
	else
		return name.find_last_of(".");
}

inline std::string path::stem() const {
	const std::string &name = filename();
	size_t p = extension_pos(name);
	if (p == std::string::npos)
		return name;
	else
		return name.substr(0, p);
}

inline std::string path::extension() const {
	const std::string &name = filename();
	size_t p = extension_pos(name);
	if (p == std::string::npos)
		return "";
	else
		return name.substr(p);
}

inline std::string path::operator [](std::size_t i) const {
	return leafs[i];
}


// Modifiers
inline void path::clear() {
	leafs.clear();
}

inline void path::set(const std::string &string) {
	leafs.clear();
	if (string.empty())
		return;			// empty case

	std::size_t p0 = 0;
#ifdef _WIN32
	const std::string delim = "\\/";

	// store volume, if any
	if (string.size() >= 2 && std::isalpha(string[0]) && string[1] == ':') {
		leafs.push_back(string.substr(0, 2));
		p0 = 2;
	}
#else
	const std::string delim = "/";
#endif

	// store root dir in generic format, if any
	std::size_t p1 = string.find_first_of(delim, p0);
	if (p1 == p0) {
		leafs.push_back("/");
		p0 = p1 + 1;
	}

	// store each path element
	p1 = string.find_first_of(delim, p0);
	if (p1 == std::string::npos) {
		if (p0 < string.length())
			leafs.push_back(string.substr(p0));	// no delimiters, only one element
	}
	else {
		for (;;) {
			if (p0 != p1) {
				leafs.push_back(string.substr(p0, p1 - p0));
			}

			p0 = p1;
			p1 = string.find_first_of(delim, ++p0);

			if (p1 == std::string::npos) {
				leafs.push_back(string.substr(p0, string.size() - p0));
				break;
			}
		}
	}

	// replace last empty entry by "."
	if (!leafs.empty() && leafs.back().empty()) {
		leafs.back() = ".";
	}
}

#ifdef _WIN32
inline void path::set(const std::wstring &wstring) {
	std::string string;

	if (!wstring.empty()) {
		int size = WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(), NULL, 0, NULL, NULL);
		string.resize(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(), &string[0], size, NULL, NULL);
	}

	this->set(string);
}
#endif

inline path path::operator /(const path &other) const {
	if (other.is_absolute()) {
		throw std::runtime_error("path::operator/(): expected a relative path!");
	}

	path result(*this);

	for (std::size_t i = 0; i<other.leafs.size(); ++i) {
		result.leafs.push_back(other.leafs[i]);
	}

	return result;
}

inline void path::push_back(std::string leaf) {
	leafs.push_back(leaf);
}

inline path& path::replace_extension(std::string new_ext) {
	// add '.' if needed
	if (!new_ext.empty() && new_ext[0] != '.')
		new_ext = std::string(".") + new_ext;

	*this = parent_path() / (stem() + new_ext);
	return *this;
}

// Iterators
inline path::iterator path::begin() {
	return leafs.begin();
}

inline path::iterator path::end() {
	return leafs.end();
}

#if __cplusplus >= 201103L || _MSC_VER >= 1800
inline path::const_iterator path::cbegin() const {
	return leafs.cbegin();
}

inline path::const_iterator path::cend() const {
	return leafs.cend();
}
#endif

// Compare
inline bool path::operator ==(const path &left) const {
	return  left.leafs == leafs;
}

inline bool path::operator <(const path &right) const {
#if __cplusplus >= 201103L || _MSC_VER >= 1800
	return std::tie(leafs) < std::tie(right.leafs);
#else
	return leafs < right.leafs;
#endif
}


// Operational functions
inline path path::absolute() const {
#ifdef _WIN32
	std::wstring value = wstr(), out(MAX_PATH, '\0');

	DWORD length = GetFullPathNameW(value.c_str(), MAX_PATH, &out[0], NULL);
	if (length == 0) {
		throw std::runtime_error("Internal error in realpath(): " + std::to_string(GetLastError()));
	}

	return path(out.substr(0, length));
#else
	char temp[PATH_MAX];
	if (realpath(str().c_str(), temp) == NULL) {
		throw std::runtime_error("Internal error in realpath(): " + std::string(strerror(errno)));
	}

	return path(temp);
#endif
}

inline bool path::exists() const {
#ifdef _WIN32
	return GetFileAttributesW(wstr().c_str()) != INVALID_FILE_ATTRIBUTES;
#else
	struct stat sb;
	return stat(str().c_str(), &sb) == 0;
#endif
}

inline std::size_t path::file_size() const {
#ifdef _WIN32
	struct _stati64 sb;
	if (_wstati64(wstr().c_str(), &sb) != 0) {
		throw std::runtime_error("path::file_size(): cannot stat file \"" + str() + "\"!");
	}
#else
	struct stat sb;
	if (stat(str().c_str(), &sb) != 0) {
		throw std::runtime_error("path::file_size(): cannot stat file \"" + str() + "\"!");
	}
#endif
	return (std::size_t) sb.st_size;
}

inline bool path::is_directory() const {
#ifdef _WIN32
	DWORD result = GetFileAttributesW(wstr().c_str());
	if (result == INVALID_FILE_ATTRIBUTES) {
		return false;
	}

	return (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat sb;
	if (stat(str().c_str(), &sb)) {
		return false;
	}

	return S_ISDIR(sb.st_mode);
#endif
}

inline bool path::is_regular_file() const {
#ifdef _WIN32
	DWORD attr = GetFileAttributesW(wstr().c_str());
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
	struct stat sb;
	if (stat(str().c_str(), &sb)) {
		return false;
	}

	return S_ISREG(sb.st_mode);
#endif
}

inline bool path::is_file() const {
	return is_regular_file();
}

inline path path::resolve(bool tryabsolute) {
	path result(*this);

	if (tryabsolute && this->exists()) {
		result = this->absolute();
	}

	for (iterator itr = result.leafs.begin(); itr != result.leafs.end();) {
		if (*itr == ".") {
			itr = result.leafs.erase(itr);
			continue;
		}

		if (*itr == "..") {
			if (itr == result.leafs.begin()) {
				if (result.is_absolute()) {
					itr = result.leafs.erase(itr);
				}
				else {
					++itr;
				}
			}
			else if (*(itr - 1) != "..") {
				itr = result.leafs.erase(itr - 1);
				itr = result.leafs.erase(itr);
			}
			else {
				++itr;
			}
			continue;
		}

		++itr;
	}

	return result;
}

inline path path::resolve(path to) {
	path result = this->resolve();

	if (!result.is_absolute()) {
		throw std::runtime_error("path::resolve(): this path must be absolute, must exist so absolute() works");
	}

	if (to.is_absolute()) {
		return to;
	}

	for (iterator itr = to.leafs.begin(); itr != to.leafs.end();++itr) {
		std::string leaf = *itr;
		if (leaf == ".") {
			continue;
		}

		if (leaf == ".." && result.leafs.size()) {
			result.leafs.pop_back();
			continue;
		}

		result.leafs.push_back(leaf);
	}

	return result;
}

inline bool path::remove_file() {
#ifdef _WIN32
	return DeleteFileW(wstr().c_str()) != 0;
#else
	return std::remove(str().c_str()) == 0;
#endif
}

inline bool path::resize_file(std::size_t target_length) {
#ifdef _WIN32
	HANDLE handle = CreateFileW(wstr().c_str(), GENERIC_WRITE, 0, nullptr, 0, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	LARGE_INTEGER size;
	size.QuadPart = (LONGLONG)target_length;

	if (SetFilePointerEx(handle, size, NULL, FILE_BEGIN) == 0) {
		CloseHandle(handle);
		return false;
	}

	if (SetEndOfFile(handle) == 0) {
		CloseHandle(handle);
		return false;
	}

	CloseHandle(handle);
	return true;
#else
	return ::truncate(str().c_str(), (off_t)target_length) == 0;
#endif
}

inline path path::get_cwd() {
#ifdef _WIN32
	std::wstring temp(MAX_PATH, '\0');
	if (!_wgetcwd(&temp[0], MAX_PATH)) {
		throw std::runtime_error("Internal error in get_cwd(): " + std::to_string(GetLastError()));
	}

	return path(temp.c_str());
#else
	char temp[PATH_MAX];
	if (::getcwd(temp, PATH_MAX) == NULL) {
		throw std::runtime_error("Internal error in get_cwd(): " + std::string(strerror(errno)));
	}

	return path(temp);
#endif
}

inline bool path::mkdirp() const {
	if (!this->is_absolute()) {
		throw std::runtime_error("path must be absolute to mkdirp()");
	}

	if (this->exists()) {
		return true;
	}

	path parent = this->dirname();
	if (!parent.exists()) {
		if (!parent.mkdirp()) {
			return false;
		}
	}

	return create_directory(*this);
}


inline bool create_directory(const path& p) {
#if defined(_WIN32)
	return CreateDirectoryW(p.wstr().c_str(), NULL) != 0;
#else
	return mkdir(p.str().c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == 0;
#endif
}

NAMESPACE_END(filesystem)
