/*
 * FileSystem.h
 *
 *  Created on: Apr 11, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_FILESYSTEM_H_
#define SRC_CIRCUIT_UTIL_FILESYSTEM_H_

#include "spring/SpringCallback.h"

#include "DataDirs.h"
#include "File.h"
#include "SkirmishAI.h"
#include "Info.h"

namespace utils {

using namespace springai;

#ifdef _WIN32
	#define SLASH "\\"
#else
	#define SLASH "/"
#endif

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

	for (std::string::size_type i = 0; i < cleaned.size(); i++) {
		if (!IsFSGoodChar(cleaned[i])) {
			cleaned[i] = '_';
		}
	}

	return cleaned;
}

static inline bool LocatePath(circuit::COOAICallback* clb, std::string& filename)
{
	DataDirs* datadirs = clb->GetDataDirs();
	static const size_t absPath_sizeMax = 2048;
	char absPath[absPath_sizeMax];
	const bool dir = !filename.empty() && (*filename.rbegin() == '/' || *filename.rbegin() == '\\');
	const bool located = datadirs->LocatePath(absPath, absPath_sizeMax, filename.c_str(), false /*writable*/, false /*create*/, dir, false /*common*/);
	delete datadirs;
	if (located) {
		filename = absPath;
	}
	return located;
}

static inline bool FileExists(circuit::COOAICallback* clb, const std::string& filename)
{
	File* file = clb->GetFile();
	const bool exists = file->GetSize(filename.c_str()) > 0;
	delete file;
	return exists;
}

static inline std::string ReadFile(circuit::COOAICallback* clb, const std::string& filename)
{
	File* file = clb->GetFile();
	std::string content;
	int fileSize = file->GetSize(filename.c_str());
	if (fileSize > 0) {
		content.resize(fileSize);
		file->GetContent(filename.c_str(), &content[0], fileSize);
	}
	delete file;
	return content;
}

static inline std::string GetAIDataGameDir(SkirmishAI* skirm, const std::string& subdir)
{
	Info* info = skirm->GetInfo();
	const char* version = info->GetValueByKey("version");
	const char* name = info->GetValueByKey("shortName");
	delete info;
	return std::string("LuaRules/Configs/") + name + SLASH + version + SLASH + subdir + SLASH;
}

} // namespace utils

#endif // SRC_CIRCUIT_UTIL_FILESYSTEM_H_
