/*
 * MetalSpot.h
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#ifndef METALSPOT_H_
#define METALSPOT_H_

#include <sys/types.h>

namespace circuit {

class CMetalSpot {
public:
	CMetalSpot(const char* setupMetal);
	virtual ~CMetalSpot();

	static void CreateInstance(const char* setupMetal);
	static CMetalSpot& GetInstance();
	static void DestroyInstance();

private:
	static CMetalSpot* singleton;
	static uint counter;
};

#define metalSpots CMetalSpot::GetInstance()

} // namespace circuit

#endif // METALSPOT_H_
