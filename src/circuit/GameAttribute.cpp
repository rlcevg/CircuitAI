/*
 * GameAttribute.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: rlcevg
 */

#include "GameAttribute.h"

namespace circuit {

std::unique_ptr<CGameAttribute> CGameAttribute::singleton(nullptr);
uint CGameAttribute::counter = 0;

CGameAttribute::CGameAttribute() :
		startBoxes(nullptr),
		metalSpots(nullptr)
{
}

CGameAttribute::~CGameAttribute()
{
}

void CGameAttribute::CreateInstance()
{
	if (singleton == nullptr) {
		singleton = std::unique_ptr<CGameAttribute>(new CGameAttribute());
	}
	counter++;
}

CGameAttribute& CGameAttribute::GetInstance()
{
	return *singleton;
}

void CGameAttribute::DestroyInstance()
{
	if (counter <= 1) {
		if (singleton != nullptr) {
			singleton = nullptr;
			// deletes singleton here;
		}
		counter = 0;
	} else {
		counter--;
	}
}

void CGameAttribute::parseSetupScript(const char* setupScript, int width, int height)
{
	if (startBoxes == nullptr) {
		startBoxes = std::make_shared<CStartBox>(setupScript, width, height);
	}
}

void CGameAttribute::parseMetalSpots(const char* setupMetal)
{
	if (metalSpots == nullptr) {
		metalSpots = std::make_shared<CMetalSpot>(setupMetal);
	}
}

CStartBox& CGameAttribute::GetStartBoxes()
{
	return *startBoxes;
}

CMetalSpot& CGameAttribute::GetMetalSpots()
{
	return *metalSpots;
}

} // namespace circuit
