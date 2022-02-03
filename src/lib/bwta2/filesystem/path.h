/*
	path.h -- A simple class for manipulating paths on Linux/Windows/Mac OS

	Copyright (c) 2015 Wenzel Jakob <wenzel@inf.ethz.ch>

	All rights reserved. Use of this source code is governed by a
	BSD-style license that can be found in the LICENSE file.

	Modified by https://github.com/pauloscustodio/filesystem
*/

#pragma once

#include "fwd.h"
#include <string>
#include <vector>
#include <iostream>

NAMESPACE_BEGIN(filesystem)

inline bool create_directory(const path&);

/**
 * \brief Simple class for manipulating paths on Linux/Windows/Mac OS
 *
 * This class is just a temporary workaround to avoid the heavy boost
 * dependency until boost::filesystem is integrated into the standard template
 * library at some point in the future.
 */
class path {
#if __cplusplus >= 201103L || _MSC_VER >= 1800
	friend std::hash<class filesystem::path>;
#endif

public:
	typedef std::string value_type;
	typedef std::vector<std::string>::iterator iterator;
#if __cplusplus >= 201103L || _MSC_VER >= 1800
	typedef std::vector<std::string>::const_iterator const_iterator;
#endif

	// Constructors
	path();
	path(const path &path);
#if __cplusplus >= 201103L || _MSC_VER >= 1800
	path(path &&path);
#endif

	path(const char *string);
	path(const std::string &string);
#ifdef _WIN32
	path(const std::wstring &wstring);
	path(const wchar_t *wstring);
#endif

	path &operator =(const path &path);
#if __cplusplus >= 201103L || _MSC_VER >= 1800
	path &operator =(path &&path);
#endif
#ifdef _WIN32
	path &operator =(const std::wstring &str);
#endif

	// Observers
	std::string str() const;			// use native separator: '\\' on Win32, else '/'
	std::string str_generic() const;	// use always '/'
#ifdef _WIN32
	std::wstring wstr() const;
#endif
	std::size_t length() const;
	bool empty() const;
	bool is_absolute() const;
	path parent_path() const;
	path dirname() const;				// alias to parent_path
	std::string filename() const;
	std::string basename() const;		// alias to filename
	std::string stem() const;
	std::string extension() const;
	std::string operator [](std::size_t i) const;

	// Modifiers
	void clear();
	void set(const std::string &string);
#ifdef _WIN32
	void set(const std::wstring &wstring);
#endif
	path& replace_extension(std::string new_ext = "");
	path operator /(const path &other) const;
	void push_back(std::string leaf);

	// Iterators
	iterator begin();
	iterator end();
#if __cplusplus >= 201103L || _MSC_VER >= 1800
	const_iterator cbegin() const;
	const_iterator cend() const;
#endif

	// Compare
	bool operator ==(const path &left) const;
	bool operator <(const path &right) const;

	// Operational functions - query or modify files, including directories, in external storage
	path absolute() const;
	bool exists() const;
	std::size_t file_size() const;
	bool is_directory() const;
	bool is_regular_file() const;
	bool is_file() const;					// alias to is_regular_file()
	path resolve(bool tryabsolute = true);	// resolve . and .., return absoulte file if it exists
	path resolve(path to);
	bool remove_file();
	bool resize_file(std::size_t target_length);
	static path get_cwd();
	bool mkdirp() const;

	// Output
	friend std::ostream &operator <<(std::ostream &os, const path &path) {
		os << path.str();
		return os;
	}

protected:
	std::vector<std::string> leafs;

	std::string str(char separator) const;
	bool has_volume() const;
	static std::size_t extension_pos(const std::string& name);
};

NAMESPACE_END(filesystem)


#if __cplusplus >= 201103L || _MSC_VER >= 1800
// hash support
NAMESPACE_BEGIN(std)

template <>
struct hash<::filesystem::path> {
	typedef ::filesystem::path argument_type;
	typedef std::size_t result_type;

	result_type operator()(const ::filesystem::path &path) const {
		std::size_t seed { 0 };
		for (const string &s : path.leafs) {
			hash_combine(seed, s);
		}
		return seed;
	}

private:
	template <class T>
	static inline void hash_combine(std::size_t& seed, const T& v) {
	    std::hash<T> hasher;
	    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
	}
};

NAMESPACE_END(std)
#endif

// load implementation
#include "path_impl.h"
