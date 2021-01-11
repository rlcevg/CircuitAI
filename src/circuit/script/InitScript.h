/*
 * InitScript.h
 *
 *  Created on: May 13, 2020
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_
#define SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_

#include "script/Script.h"
#include "util/MaskHandler.h"

#include <string>
#include <map>

namespace springai {
	class AIFloat3;
}

class asIScriptFunction;
class CScriptArray;

namespace circuit {

class CCircuitAI;

class CInitScript: public IScript {
public:
	struct SInitInfo {
		SInitInfo() {}
		SInitInfo(const SInitInfo& o);
		~SInitInfo();
		struct SCategoryInfo {
			SCategoryInfo() {}
			~SCategoryInfo() {}
			std::string air;
			std::string land;
			std::string water;
			std::string bad;
			std::string good;
		} category;
		CScriptArray* profile = nullptr;  // parts
	};

	CInitScript(CScriptManager* scr, CCircuitAI* circuit);
	virtual ~CInitScript();

	void InitConfig(const std::string& profile,
			std::vector<std::string>& outCfgParts);
	void Init() override;

	void RegisterMgr();

private:
	CMaskHandler::TypeMask AddRole(const std::string& name, int actAsRole);

	void Log(const std::string& msg) const;
	void AddPoint(const springai::AIFloat3& pos, const std::string& msg) const;
	void DelPoint(const springai::AIFloat3& pos) const;
	void Pause(bool enable, const std::string& msg) const;
	int Dice(const CScriptArray* array) const;
	template<typename T> T Min(T l, T r) const { return std::min(l, r); }
	template<typename T> T Max(T l, T r) const { return std::max(l, r); }

	CCircuitAI* circuit;
	std::string folderName;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_INITSCRIPT_H_
