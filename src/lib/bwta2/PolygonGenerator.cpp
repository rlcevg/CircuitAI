#include "PolygonGenerator.h"

#include "Utils.h"

namespace BWTA
{
	struct holeLabel_t {
		Contour ring;
		int labelID;
		holeLabel_t(Contour _ring, int id) : ring(_ring), labelID(id) {}
	};

	// To vectorize obstacles (unwalkable areas) we use the algorithm described in:
	// "A Linear-Time Component-Labeling Algorithm Using Contour Tracing Technique" Chang et al.
	// http://ocrwks11.iis.sinica.edu.tw/~dar/Download/Papers/Component/component_labeling_cviu.pdf
	// This algorithm produce a component-label map and extract the external/internal contours needed to build the polygon

	int8_t searchDirection[8][2] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }, { -1, 0 }, { -1, 1 } };

	void tracer(int& cy, int& cx, int& tracingdirection, RectangleArray<int>& labelMap, const RectangleArray<bool>& bitMap)
	{
		int i, y, x;
		int width = bitMap.getWidth();
		int height = bitMap.getHeight();

		for (i = 0; i < 7; ++i) {
			y = cy + searchDirection[tracingdirection][0];
			x = cx + searchDirection[tracingdirection][1];

			// filter invalid position (out of range)
			if (x < 0 || x >= height || y < 0 || y >= width) {
				tracingdirection = (tracingdirection + 1) % 8;
				continue;
			}
			if (bitMap[y][x]) {
				labelMap[y][x] = -1;
				tracingdirection = (tracingdirection + 1) % 8;
			} else {
				cy = y;
				cx = x;
				break;
			}
		}
	}

	Contour contourTracing(int cy, int cx, const size_t& labelId, int tracingDirection, RectangleArray<int>& labelMap, const RectangleArray<bool>& bitMap)
	{
		bool tracingStopFlag = false, keepSearching = true;
		int fx, fy, sx = cx, sy = cy;
		Contour contourPoints;

		tracer(cy, cx, tracingDirection, labelMap, bitMap);
		contourPoints.emplace_back(cy, cx);

		if (cx != sx || cy != sy) {
			fx = cx;
			fy = cy;

			while (keepSearching) {
				tracingDirection = (tracingDirection + 6) % 8;
				labelMap[cy][cx] = labelId;
				tracer(cy, cx, tracingDirection, labelMap, bitMap);
				contourPoints.emplace_back(cy, cx);

				if (cx == sx && cy == sy) {
					tracingStopFlag = true;
				} else if (tracingStopFlag) {
					if (cx == fx && cy == fy) {
						keepSearching = false;
					} else {
						tracingStopFlag = false;
					}
				}
			}
		}
		return contourPoints;
	}

	// given a bitmap (a walkability map in our context) it returns the external contour of obstacles
	void connectedComponentLabeling(std::vector<Contour>& contours, const RectangleArray<bool>& bitMap, RectangleArray<int>& labelMap)
	{
		int cy, cx, tracingDirection, connectedComponentsCount = 0, labelId = 0;
		int width = bitMap.getWidth();
		int height = bitMap.getHeight();
		std::vector<holeLabel_t> holesToLabel;

		for (cy = 0; cy < width; ++cy) {
			for (cx = 0, labelId = 0; cx < height; ++cx) {
				if (!bitMap[cy][cx]) {
					if (labelId != 0) { // use pre-pixel label
						labelMap[cy][cx] = labelId;
					} else {
						labelId = labelMap[cy][cx];

						if (labelId == 0) {
							labelId = ++connectedComponentsCount;
							tracingDirection = 0;
							// external contour
							contours.push_back(contourTracing(cy, cx, labelId, tracingDirection, labelMap, bitMap));
							labelMap[cy][cx] = labelId;
						}
					}
				} else if (labelId != 0) { // walkable & pre-pixel has been labeled
					if (labelMap[cy][cx] == 0) {
						tracingDirection = 1;
						// internal contour
						Contour hole = contourTracing(cy, cx - 1, labelId, tracingDirection, labelMap, bitMap);
						BoostPolygon polygon;
						boost::geometry::assign_points(polygon, hole);
						// if polygon isn't too small, add it to the result
						if (boost::geometry::area(polygon) > MIN_ARE_POLYGON) {
							// TODO a polygon can have walkable polygons as "holes", save them
							LOG(" - [WARNING] Found big walkable HOLE");
						} else {
							// "remove" the hole filling it with the polygon label
							holesToLabel.emplace_back(hole, labelId);
						}
					}
					labelId = 0;
				}
			}
		}

		RectangleArray<bool> nodeMap(width, height);
		nodeMap.setTo(false);
		for (auto& holeToLabel : holesToLabel) {
			scanLineFill(holeToLabel.ring, holeToLabel.labelID, labelMap, nodeMap);
		}
	}

	// anchor vertices near borders of the map to the border
	// used to fix errors from simplify polygon
	void anchorToBorder(BoostPolygon& polygon, const int maxX, const int maxY, const int maxMarginX, const int maxMarginY)
	{
		bool modified = false;
		for (auto& vertex : polygon.outer()) {
			if (vertex.x() <= ANCHOR_MARGIN) { modified = true; vertex.x(0); }
			if (vertex.y() <= ANCHOR_MARGIN) { modified = true; vertex.y(0); }
			if (vertex.x() >= maxMarginX) { modified = true; vertex.x(maxX); }
			if (vertex.y() >= maxMarginY) { modified = true; vertex.y(maxY); }
		}
		// after anchoring we simplify again the polygon to remove unnecessary points
		if (modified) {
			BoostPolygon simPolygon;
			boost::geometry::simplify(polygon, simPolygon, 1.0);
			polygon = simPolygon;
		}
		
// 		std::vector<BoostPolygon> output;
// 		boost::geometry::dissolve(polygon, output);
// 		if (output.size() != 1) {
// 			LOG("ERROR: polygon simplification generated " << output.size()  << " polygons");
// 		} else {
// 			boost::geometry::simplify(output.at(0), polygon, 1.0);
// 		}
	}

	bool isTouchingMapBorder(Contour contour, int  maxX, int maxY)
	{
		for (const auto& point : contour) {
			if (point.x() == 0 || point.x() == maxX ||
				point.y() == 0 || point.y() == maxY)
				return true;
		}
		return false;
	}


	void generatePolygons(std::vector<BoostPolygon>& polygons, RectangleArray<int>& labelMap)
	{
		Timer timer;
		timer.start();

		std::vector<Contour> contours;
		connectedComponentLabeling(contours, MapData::walkability, labelMap);
// 		labelMap.saveToFile(std::string(BWTA_PATH)+"labelMap.txt");

		LOG(" - Component-Labeling Map and Contours extracted in " << timer.stopAndGetTime() << " seconds");
		timer.start();

		const int maxX = MapData::walkability.getWidth() - 1;
		const int maxY = MapData::walkability.getHeight() - 1;
		const int maxMarginX = maxX - ANCHOR_MARGIN;
		const int maxMarginY = maxY - ANCHOR_MARGIN;

		RectangleArray<bool> nodeMap(labelMap.getWidth(), labelMap.getHeight());
		nodeMap.setTo(false);

// 		boost::geometry::model::multi_polygon<BoostPolygon> polygons;
// 		const auto& contour = contours.at(29);
		for (const auto& contour : contours) {
			BoostPolygon polygon, simPolygon;
			boost::geometry::assign_points(polygon, contour);
			bool touchingMapBroder = isTouchingMapBorder(contour, maxX, maxY);
			auto polArea = boost::geometry::area(polygon);

			const auto& pLabel = polygon.outer().at(0);
			int labelID = labelMap[static_cast<int>(pLabel.x())][static_cast<int>(pLabel.y())];

			// if polygon isn't too small, add it to the result
			if ((touchingMapBroder && polArea > MIN_ARE_POLYGON) || 
				(!touchingMapBroder && polArea > MIN_ARE_INNER_POLYGON)) {

//				if (labelID == 3) LOG(" - polygon " << boost::geometry::dsv(polygon));

				// If the starting-ending points are co-linear, this is a special case that is not simplified
				// http://boost-geometry.203548.n3.nabble.com/Simplifying-polygons-with-co-linear-points-td3415757.html
				// To avoid problems with borders, if the initial point is in the border, we rotate the points
				// until we find one that it is not in the border (or all points explored)
				// Notice that we may still have co-linear points, but hopefully not in the border.
				const auto& p0 = polygon.outer().at(0);
				if (p0.x() <= 0 || p0.x() >= maxX || p0.y() <= 0 || p0.y() >= maxY) {
					// find index of not border point
					size_t index = 0;
					for (size_t i = 1; i < polygon.outer().size(); ++i) {
						const auto& p1 = polygon.outer().at(i);
						if (p1.x() > ANCHOR_MARGIN*2 && p1.x() < maxX && p1.y() > ANCHOR_MARGIN*2 && p1.y() < maxY) {
							// not border point found
							index = i;
							break;
						}
					}
					if (index != 0) {
						auto& outerRing = polygon.outer();
						std::rotate(outerRing.begin(), outerRing.begin() + index, outerRing.end());
						outerRing.push_back(outerRing.at(0));
					}
// 					LOG(" - Rotated polygon " << boost::geometry::dsv(simPolygon));
				}

				// Uses Douglas-Peucker algorithm to simplify points in the polygon
				// https://en.wikipedia.org/wiki/Ramer%E2%80%93Douglas%E2%80%93Peucker_algorithm
				boost::geometry::simplify(polygon, simPolygon, 2.0);
//				if (labelID == 3) LOG(" - Simplified polygon " << boost::geometry::dsv(simPolygon));

				anchorToBorder(simPolygon, maxX, maxY, maxMarginX, maxMarginY);
//				if (labelID == 3) LOG(" -   Anchored polygon " << boost::geometry::dsv(simPolygon));

				if (!boost::geometry::is_simple(simPolygon)) {
					LOG("[Error] polygon " << labelID << " not simple!!!!!!!!!!!!!!");
				}
				if (!boost::geometry::is_valid(simPolygon)) { // TODO new Boost version has message
					LOG("[Error] polygon " << labelID << " not valid!!!!!!!!!!!!!!");
				} else {
					polygons.push_back(simPolygon);
				}
			} else {
				// region discarded, relabel
// 				LOG("Discarded obstacle with label : " << labelID << " and area: " << polArea);
				scanLineFill(contour, 0, labelMap, nodeMap, true);
			}
		}

//		labelMap.saveToFile(std::string(BWTA_PATH)+"labelMap.txt");

		LOG(" - Vectorized areas in " << timer.stopAndGetTime() << " seconds");

	}
}
