
#ifdef DEBUG_DRAW
#include "Painter.h"
#endif

#include "LoadData.h"
#include "BaseLocationGenerator.h"
#include "PolygonGenerator.h"
#include "RegionGenerator.h"
#include "ClosestObjectMap.h"
#include "filesystem/path.h"

namespace BWTA
{
	void analyze_map();

	void analyze()
	{
		cleanMemory();
		Timer timer;

#ifndef OFFLINE
		loadMapFromBWAPI();
#endif
		
		loadMap(); // compute extra map info

		// Verify if "BWTA2" directory exists, and create it if it doesn't.
		std::string bwtaPath(BWTA_PATH);
		filesystem::path bwtaFolder(filesystem::path::get_cwd() / bwtaPath);
		if (!bwtaFolder.exists()) bwtaFolder.mkdirp();

		std::string filename = bwtaPath + MapData::hash + ".bwta";

//		if (isFileVersionCorrect(filename)) {
//			LOG("Recognized map, loading map data...");
//
//			timer.start();
//			load_data(filename);
//			LOG("Loaded map data in " << timer.stopAndGetTime() << " seconds");
//		} else {
			LOG("Analyzing new map...");

			timer.start();
			analyze_map();
			LOG("Map analyzed in " << timer.stopAndGetTime() << " seconds");

// 			save_data(filename);
// 			LOG("Saved map data.");
//		}

#ifndef OFFLINE
		attachResourcePointersToBaseLocations(BWTA_Result::baselocations);
#endif
	}

	void analyze_map()
	{
#ifdef DEBUG_DRAW
		int argc = 1;
		char* argv = "0";
		QGuiApplication a(argc, &argv); // needed for print text (init fonts)
		const Painter::Scale imageScale = Painter::Scale::Walk;
//		const Painter::Scale imageScale = Painter::Scale::Pixel;
		Painter painter(imageScale);
#endif
		Timer timer;
		timer.start();

		std::vector<BoostPolygon> boostPolygons;
		BWTA_Result::obstacleLabelMap.resize(MapData::walkability.getWidth(), MapData::walkability.getHeight());
		BWTA_Result::obstacleLabelMap.setTo(0);
		generatePolygons(boostPolygons, BWTA_Result::obstacleLabelMap);

		// translate Boost polygons to BWTA polygons
		for (const auto& pol : boostPolygons) {
			Polygon* bwtaPol = new PolygonImpl(pol);
			BWTA_Result::unwalkablePolygons.push_back(bwtaPol);
		}

		LOG(" [Detected polygons in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.render("01-Polygons");
		// Prints each polygon individually to debug 
// 		for (auto tmpPol : polygons) {
// 			painter.drawPolygon(tmpPol, QColor(180, 180, 180));
// 			painter.render();
// 		}
#endif
		timer.start();

		RegionGraph graph;
		bgi::rtree<BoostSegmentI, bgi::quadratic<16> > rtree;
		generateVoronoid(BWTA_Result::unwalkablePolygons, BWTA_Result::obstacleLabelMap, graph, rtree);
		
		LOG(" [Computed Voronoi in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawGraph(graph, Painter::Scale::Walk, imageScale);
		painter.render("02-Voronoi");
#endif
		timer.start();

		pruneGraph(graph);

		LOG(" [Pruned Voronoi in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawGraph(graph, Painter::Scale::Walk, imageScale);
		painter.render("03-VoronoiPruned");
#endif
		timer.start();

		detectNodes(graph, BWTA_Result::unwalkablePolygons);

		LOG(" [Identified region/chokepoints nodes in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawGraph(graph, Painter::Scale::Walk, imageScale);
		painter.drawNodes(graph, graph.regionNodes, Qt::blue, Painter::Scale::Walk, imageScale);
		painter.drawNodes(graph, graph.chokeNodes, Qt::red, Painter::Scale::Walk, imageScale);
		painter.render("05-NodesDetected");
#endif
		timer.start();

		RegionGraph graphSimplified;
		simplifyGraph(graph, graphSimplified);

		LOG(" [Simplified graph in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawGraph(graphSimplified, Painter::Scale::Walk, imageScale);
		painter.drawNodes(graphSimplified, graphSimplified.regionNodes, Qt::blue, Painter::Scale::Walk, imageScale);
		painter.drawNodes(graphSimplified, graphSimplified.chokeNodes, Qt::red, Painter::Scale::Walk, imageScale);
		painter.render("06-GraphPruned");
#endif
		timer.start();

		mergeRegionNodes(graphSimplified);

		LOG(" [Merged consecutive region nodes in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawGraph(graphSimplified, Painter::Scale::Walk, imageScale);
		painter.drawNodes(graphSimplified, graphSimplified.regionNodes, Qt::blue, Painter::Scale::Walk, imageScale);
		painter.drawNodes(graphSimplified, graphSimplified.chokeNodes, Qt::red, Painter::Scale::Walk, imageScale);
		painter.render("07-GraphMerged");
#endif
		timer.start();

		std::map<nodeID, chokeSides_t> chokepointSides;
		getChokepointSides(graphSimplified, rtree, chokepointSides);

		LOG(" [Chokepoints sides computed in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawGraph(graphSimplified, Painter::Scale::Walk, imageScale);
		painter.drawNodes(graphSimplified, graphSimplified.regionNodes, Qt::blue, Painter::Scale::Walk, imageScale);
		painter.drawChokepointsSides(chokepointSides, Qt::red, Painter::Scale::Walk, imageScale);
		painter.render("08-WallOffChokepoints");
#endif
		timer.start();

		std::vector<BoostPolygon> polReg;
		createRegionsFromGraph(boostPolygons, BWTA_Result::obstacleLabelMap, graphSimplified, chokepointSides,
			BWTA_Result::regions, BWTA_Result::chokepoints, polReg);

		LOG(" [Created BWTA regions/chokepoints in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawRegions(BWTA_Result::regions, Painter::Scale::Pixel, imageScale);
		painter.drawChokepoints(BWTA_Result::chokepoints, imageScale);
		painter.render("09-Regions");
#endif
		timer.start();

		detectBaseLocations(BWTA_Result::baselocations);
// 		for (auto i : BWTA_Result::baselocations) {
// 			log("BaseLocation at Position " << i->getPosition() << " Tile " << i->getTilePosition());
// 		}

		LOG(" [Calculated base locations in " << timer.stopAndGetTime() << " seconds]");
		timer.start();

		computeAllClosestObjectMaps();

		LOG(" [Calculated closest maps in " << timer.stopAndGetTime() << " seconds]");
		timer.start();

		calculateBaseLocationProperties();
// 		log("Debug BaseLocationProperties");
// 		for (const auto& base : BWTA_Result::baselocations) {
// 			BaseLocationImpl* baseI = (BaseLocationImpl*)base;
// 			log("Base Position" << baseI->getTilePosition() << ",isIsland:" << baseI->isIsland()
// 				<< ",isStartLocation:" << baseI->isStartLocation()
// 				<< ",regionCenter" << baseI->getRegion()->getCenter());
// 		}

		LOG(" [Calculated base location properties in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
//		painter.drawClosestBaseLocationMap(BWTA_Result::getBaseLocationW, BWTA_Result::baselocations);
//		painter.render("ClosestBaseLocationMap");
//		painter.drawClosestChokepointMap(BWTA_Result::getChokepointW, BWTA_Result::chokepoints);
//		painter.render("ClosestChokepointMap");
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawRegions(BWTA_Result::regions, Painter::Scale::Pixel, imageScale);
		painter.drawChokepoints(BWTA_Result::chokepoints, imageScale);
		painter.drawBaseLocations(BWTA_Result::baselocations, imageScale);
		painter.render("10-Final");
#endif
		timer.start();

		// compute coverage points of each region
		// TODO tile res should be enough
		const int SIGHT_RANGE = 7 * 2 * 4; // SCV = 7 tiles * 2, transformed into walk tiles
		for (auto region : BWTA_Result::regions) {
			RegionImpl* r = dynamic_cast<RegionImpl*>(region);
			// first coverage point is openness point
			BWAPI::WalkPosition pos(r->_opennessPoint);
			r->_coveragePositions.push_back(pos);
			int regionID = BWTA_Result::regionLabelMap[pos.x][pos.y];

			// compute max lenght of the bounding box
			int minX = std::numeric_limits<int>::max();
			int minY = std::numeric_limits<int>::max();
			int maxX = 0;
			int maxY = 0;
			for (const auto& p : r->_polygon) {
				minX = std::min(minX, p.x);
				minY = std::min(minY, p.y);
				maxX = std::max(maxX, p.x);
				maxY = std::max(maxY, p.y);
			}
			int maxBoundingBoxLength = std::max(maxX-minX, maxY-minY);
			maxBoundingBoxLength /= 16; // lenght is in pixel, translate to walkTiles
			maxBoundingBoxLength *= 2;

			//searches outward in a spiral.
			int x      = pos.x;
			int y      = pos.y;
			int length = 1;
			int j      = 0;
			bool first = true;
			int dx     = 0;
			int dy     = 1;	
			while (length < maxBoundingBoxLength) {
				// if valid position
				if (x >= 0 && x < BWTA_Result::regionLabelMap.getWidth() && 
					y >= 0 && y < BWTA_Result::regionLabelMap.getHeight() &&
					regionID == BWTA_Result::regionLabelMap[x][y]) {
						// check if we are far enough
						int minDistance = std::numeric_limits<int>::max();
						for (const auto& coverPos : r->_coveragePositions) {
							minDistance = std::min(minDistance, std::abs(coverPos.x-x)+std::abs(coverPos.y-y) );
						}
						if (minDistance > SIGHT_RANGE) r->_coveragePositions.emplace_back(x,y);
				}

				//otherwise, move to another position
				x = x + dx;
				y = y + dy;
				//count how many steps we take in this direction
				j++;
				if (j == length) { //if we've reached the end, its time to turn
					j = 0;
					if (!first) length++;
					first =! first;
					//turn counter clockwise 90 degrees:
					if (dx == 0) {
						dx = dy;
						dy = 0;
					} else {
						dy = -dx;
						dx = 0;
					}
				}
				//Spiral out. Keep going.
			}
		}

		LOG(" [Calculated region coverage positions in " << timer.stopAndGetTime() << " seconds]");
#ifdef DEBUG_DRAW
		painter.drawPolygons(BWTA_Result::unwalkablePolygons, Painter::Scale::Walk, imageScale);
		painter.drawRegions(BWTA_Result::regions, Painter::Scale::Pixel, imageScale);
		painter.drawChokepoints(BWTA_Result::chokepoints, imageScale);
		painter.drawCoverPoints(BWTA_Result::regions, imageScale);
		painter.render("11-CoverPoints");
#endif

	}

}