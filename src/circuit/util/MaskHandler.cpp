/*
 * MaskHandler.cpp
 *
 *  Created on: Apr 7, 2019
 *      Author: rlcevg
 */

#include "util/MaskHandler.h"

#include <sstream>

namespace circuit {

CMaskHandler::CMaskHandler()
{
	Init();
}

CMaskHandler::~CMaskHandler()
{
}

void CMaskHandler::Init()
{
	masks.reserve(GetMaxMasks());
	firstUnused = 0;
}

void CMaskHandler::Release()
{
	masks.clear();
}

std::string CMaskHandler::GetName(Type type) const
{
	for (const auto& kv : masks) {
		if (type == kv.second.type) {
			return kv.first;
		}
	}
	return "";
}

CMaskHandler::TypeMask CMaskHandler::GetTypeMask(const std::string& name)
{
	TypeMask tm(-1, 0);

	// the empty mask
	if (name.empty()) {
		return tm;
	}

	auto it = masks.find(name);
	if (it == masks.end()) {
		// this mask is yet unknown
		if (firstUnused >= CMaskHandler::GetMaxMasks()) {
			return tm;
		}

		// create the mask (bit field value)
		tm.type = firstUnused;
		tm.mask = GetMask(firstUnused);

		masks[name] = tm;
		++firstUnused;
	} else {
		// this mask is already known
		return it->second;
	}

	return tm;
}

CMaskHandler::Mask CMaskHandler::GetMasks(const std::string& names)
{
	Mask ret = 0;

	// split on ' '
	std::stringstream namesStream(names);
	std::string name;

	while (std::getline(namesStream, name, ' ')) {
		if (name.empty()) {
			continue;
		}

		ret |= GetMask(name);
	}

	return ret;
}

std::vector<std::string> CMaskHandler::GetMaskNames(Mask bits) const
{
	std::vector<std::string> names;

	names.reserve(masks.size());

	for (unsigned int bit = 1; bit != 0; bit = (bit << 1)) {
		if ((bit & bits) == 0) {
			continue;
		}

		for (auto it = masks.cbegin(); it != masks.cend(); ++it) {
			if (it->second.mask != bit) {
				continue;
			}

			names.push_back(it->first);
		}
	}

	return names;
}

} // namespace circuit
