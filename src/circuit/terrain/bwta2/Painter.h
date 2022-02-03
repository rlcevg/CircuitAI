#pragma once

#ifdef DEBUG_DRAW
#include <QtGui/QPainter>
#include <QtGui/QColor>
#include <QtGui/QGuiApplication>

#include "MapData.h"
#include "RegionGenerator.h"

namespace BWTA {
	class Painter {
	public:
		enum Scale { Pixel, Walk, Build};

		Painter(Scale mapScale);
		~Painter() {}

		void render(const std::string& label = std::string());

		void drawPixelMapBorder() { drawMapBorder(MapData::mapWidthPixelRes - 1, MapData::mapHeightPixelRes - 1); }
		void drawWalkMapBorder() { drawMapBorder(MapData::mapWidthWalkRes - 1, MapData::mapHeightWalkRes - 1); }
		void drawBuildMapBorder() { drawMapBorder(MapData::mapWidthTileRes - 1, MapData::mapHeightTileRes - 1); }

		void drawPolygon(const Polygon& polygon, QColor fillColor, Scale fromScale = Scale::Pixel, Scale toScale = Scale::Pixel);
		void drawPolygons(const std::vector<Polygon*>& polygons, Scale fromScale = Scale::Pixel, Scale toScale = Scale::Pixel);
		void drawRegions(const std::vector<Region*>& regions, Scale fromScale = Scale::Pixel, Scale toScale = Scale::Pixel);
		void drawCoverPoints(const std::vector<Region*>& regions, Scale toScale = Scale::Pixel);

		void drawChokepoints(const std::set<Chokepoint*>& chokepoints, Scale toScale = Scale::Pixel);
		void drawLine(const int& x1, const int& y1, const int& x2, const int& y2, double scale);
		void drawChokepointsSides(const std::map<nodeID, chokeSides_t>& chokepointSides, QColor color, Scale fromScale = Scale::Pixel, Scale toScale = Scale::Pixel);

		void drawEdges(std::vector<boost::polygon::voronoi_edge<double>> edges);
		void drawGraph(const RegionGraph& graph, Scale fromScale = Scale::Pixel, Scale toScale = Scale::Pixel);
		void drawNodes(const RegionGraph& graph, const std::set<nodeID>& nodes, QColor color, 
			Scale fromScale = Scale::Pixel, Scale toScale = Scale::Pixel, int size = 6);
		
		void drawText(int x, int y, std::string text);
		void drawBaseLocations(const std::set<BaseLocation*>& baseLocations, Scale toScale = Scale::Pixel);

		void drawClosestBaseLocationMap(RectangleArray<BaseLocation*> map, std::set<BaseLocation*> baseLocations);
		void drawClosestChokepointMap(RectangleArray<Chokepoint*> map, std::set<Chokepoint*> chokepoints);
		void drawHeatMap(RectangleArray<int> map, float maxValue);

	private:
		QImage image;
		QPainter painter;
		int renderCounter;
		uint16_t _width;
		uint16_t _height;

		void drawMapBorder(uint16_t width, uint16_t height);
		static double getScale(Scale fromScale, Scale toScale);
		void drawPolygon(const Polygon& polygon, QColor fillColor, double scale);

		void getHeatMapColor(float value, int &red, int &green, int &blue) const;
		static QColor hsl2rgb(double h, double sl, double l);
	};
}
#endif