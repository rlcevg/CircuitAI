/*
 * SpringEngine.cpp
 *
 *  Created on: Nov 8, 2019
 *      Author: rlcevg
 */

#include "spring/SpringEngine.h"

#include "SSkirmishAICallback.h"	// "direct" C API

namespace circuit {

CEngine::CEngine(const struct SSkirmishAICallback* clb, int sAIId)
		: sAICallback(clb)
		, skirmishAIId(sAIId)
{
}

CEngine::~CEngine()
{
}

const char* CEngine::GetVersionMajor() const
{
	return sAICallback->Engine_Version_getMajor(skirmishAIId);
}

} // namespace circuit
