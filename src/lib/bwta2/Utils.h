#pragma once

#include <BWTA/Polygon.h>

#include "Heap.h"
#include "MapData.h"
#include "stdafx.h"

// geometry typedefs
using BoostSegment = boost::geometry::model::segment<BoostPoint>;
using BoostSegmentI = std::pair < BoostSegment, std::size_t > ;
using Contour = std::vector<BoostPoint>;
// Voronoi typedefs
typedef boost::polygon::voronoi_diagram<double> BoostVoronoi;
typedef boost::int32_t int32;
typedef boost::polygon::point_data<int32> VoronoiPoint;
typedef boost::polygon::segment_data<int32> VoronoiSegment;

namespace BWTA
{
	int const BWTA_FILE_VERSION = 6;

	/**
	* The scanline flood fill algorithm works by intersecting scanline with polygon edges and
	* fills the polygon between pairs of intersections.
	*/
	// contour is a list of pixel points of a polygon's contour (first and last point should be the same)
	// lableID is the id to label the current polygon at labelMap
	void scanLineFill(Contour contour, const int& labelID,
		RectangleArray<int>& labelMap, RectangleArray<bool>& nodeMap, bool fillContour = false);
	// polyCorners only has the points of the polygon
	void scanLineFill(const Contour &polyCorners, const int& labelID, RectangleArray<int>& labelMap);

	void polygonBoundingBox(const Contour& contour, size_t& maxX, size_t& minX, size_t& maxY, size_t& minY);
	bool isFileVersionCorrect(std::string filename);

	template<typename T>
	inline void walkResMapToTileResMap(const RectangleArray<T*>& walkResMap, RectangleArray<T*>& tileResMap)
	{
		for (size_t x = 0; x < MapData::mapWidthTileRes; ++x) {
			for (size_t y = 0; y < MapData::mapHeightTileRes; ++y) {
//				Heap<T*, int> h;
//				for (int xi = 0; xi < 4; ++xi) {
//					for (int yi = 0; yi < 4; ++yi) {
//						T* bl = walkResMap[x * 4 + xi][y * 4 + yi];
//						if (bl == nullptr) continue;
//						if (h.contains(bl)) {
//							int n = h.get(bl) + 1;
//							h.set(bl, n);
//						} else {
//							h.push(std::make_pair(bl, 1));
//						}
//					}
//				}
//				if (!h.empty()) tileResMap[x][y] = h.top().first;
				tileResMap[x][y] = walkResMap[x][y];
			}
		}
	}

}
