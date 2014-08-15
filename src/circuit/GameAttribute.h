/*
 * GameAttribute.h
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#ifndef GAMESETUP_H_
#define GAMESETUP_H_

#include "StartBox.h"
#include "MetalSpot.h"

#include <sys/types.h>
#include <memory>

namespace circuit {

class CGameAttribute {
public:
	CGameAttribute();
	virtual ~CGameAttribute();

	static void CreateInstance();
	static CGameAttribute& GetInstance();
	static void DestroyInstance();

	void parseSetupScript(const char* setupScript, int width, int height);
	void parseMetalSpots(const char* setupMetal);
	bool IsMetalSpotsInitialized();
	CStartBox& GetStartBoxes();
	CMetalSpot& GetMetalSpots();

private:
	static std::unique_ptr<CGameAttribute> singleton;
	static uint counter;

	std::shared_ptr<CStartBox> startBoxes;
	std::shared_ptr<CMetalSpot> metalSpots;
};

#define gameAttribute CGameAttribute::GetInstance()

} // namespace circuit

#endif // GAMESETUP_H_
