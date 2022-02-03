#pragma once

#include <BWTA/Polygon.h>

#include "MapData.h"
#include "stdafx.h"

namespace BWTA
{
	const int MIN_ARE_INNER_POLYGON = 140; // polygons with an area less than this will be discarded
	const int MIN_ARE_POLYGON = 40; // polygons with an area less than this will be discarded
	const int ANCHOR_MARGIN = 2; // margin to anchor a point into the borders of the map

	void generatePolygons(std::vector<BoostPolygon>& polygons, RectangleArray<int>& labelMap);
}