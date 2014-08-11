/*
 * MetalSpot.cpp
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#include "MetalSpot.h"

namespace circuit {

CMetalSpot* CMetalSpot::singleton = nullptr;
uint CMetalSpot::counter = 0;

CMetalSpot::CMetalSpot(const char* setupMetal)
{
}

CMetalSpot::~CMetalSpot()
{
	// TODO Auto-generated destructor stub
}

void CMetalSpot::CreateInstance(const char* setupMetal)
{
	if (singleton == nullptr) {
		singleton = new CMetalSpot(setupMetal);
	}
	counter++;
}

CMetalSpot& CMetalSpot::GetInstance()
{
	return *singleton;
}

void CMetalSpot::DestroyInstance()
{
	if (counter <= 1) {
		if (singleton != nullptr) {
			// SafeDelete
			CMetalSpot* tmp = singleton;
			singleton = nullptr;
			delete tmp;
		}
		counter = 0;
	} else {
		counter--;
	}
}

} // namespace circuit
