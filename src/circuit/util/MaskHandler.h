/*
 * MaskHandler.h
 *
 *  Created on: Apr 7, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_MASKHANDLER_H_
#define SRC_CIRCUIT_UTIL_MASKHANDLER_H_

#include <string>
#include <vector>
#include <unordered_map>

namespace circuit {

class CMaskHandler {
public:
	using Type = int;
	using Mask = uint32_t;
	struct TypeMask {
		TypeMask() : TypeMask(-1, 0) {}
		TypeMask(Type t, Mask m) : type(t), mask(m) {}
		TypeMask(const TypeMask& o) : TypeMask(o.type, o.mask) {}
		Type type;
		Mask mask;
	};
	using MaskName = std::unordered_map<std::string, TypeMask>;

	CMaskHandler(const CMaskHandler&) = delete;
	CMaskHandler& operator=(const CMaskHandler&) = delete;
	CMaskHandler();
	virtual ~CMaskHandler();

	// masks work as bitfields, so
	// we can not support more than this
	static constexpr inline int GetMaxMasks() { return (sizeof(Mask) * 8); }

	void Init();
	void Release();

	bool HasType(const std::string& name) const { return masks.find(name) != masks.end(); }
	Type GetType(const std::string& name) { return GetTypeMask(name).type; }

	Mask GetMask(const std::string& name) { return GetTypeMask(name).mask; }

	static Mask GetMask(Type type) { return (1 << type); }

	std::string GetName(Type type) const;

	/**
	 * Returns the masks bit-field value.
	 * @return the masks bit-field value or 0,
	 *         in case of empty name or too many masks
	 */
	TypeMask GetTypeMask(const std::string& name);

	/**
	 * Returns the bit-field values of a list of mask names.
	 * @see GetMask(std::string name)
	 */
	Mask GetMasks(const std::string& names);

	/**
	 * Returns a list of names of all masks described by the bit-field.
	 * @see GetMask(std::string name)
	 */
	std::vector<std::string> GetMaskNames(Mask bits) const;

	const MaskName& GetMasks() const { return masks; }

private:
	// iterated in GetMaskNames; reserved size must be constant
	MaskName masks;

	Type firstUnused = 0;
};

using SideType = CMaskHandler::Type;

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_MASKHANDLER_H_
