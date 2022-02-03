#pragma once

#include "MapData.h"

namespace BWTA
{
	template<typename T>
	struct objectDistance_t {
		int x;
		int y;
		T objectRef;
		int distance;

		objectDistance_t(int xTmp, int yTmp, T ref, int dis = 0)
			: x(xTmp), y(yTmp), objectRef(ref), distance(dis) {};
	};

	using baseDistance_t = objectDistance_t<BaseLocation*> ;
	using chokeDistance_t = objectDistance_t<Chokepoint*> ;
	using labelDistance_t = objectDistance_t<int> ;

	void computeAllClosestObjectMaps();
}