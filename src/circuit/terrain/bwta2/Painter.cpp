#ifdef DEBUG_DRAW
#include "Painter.h"
#include "GraphColoring.h"
#include "BaseLocationImpl.h"

namespace BWTA {

	const std::vector<QColor> baseColors = { QColor(0, 114, 189), QColor(217, 83, 25), QColor(237, 177, 32),
		QColor(126, 47, 142), QColor(119, 172, 48), QColor(77, 190, 238), QColor(162, 20, 47) };

	const std::vector<QColor> mapColors = { QColor(180, 180, 180), QColor(204, 193, 218), QColor(230, 185, 184),
		QColor(252, 216, 181), QColor(215, 228, 189), QColor(77, 190, 238), QColor(162, 20, 47) };

	Painter::Painter(Scale mapScale) :
		renderCounter(1), _width(MapData::mapWidthPixelRes), _height(MapData::mapHeightPixelRes)
	{
		if (mapScale == Scale::Walk) {
			_width = MapData::mapWidthWalkRes;
			_height = MapData::mapHeightWalkRes;
		} else if (mapScale == Scale::Build) {
			_width = MapData::mapWidthTileRes;
			_height = MapData::mapHeightTileRes;
		}

		image = QImage(_width, _height, QImage::Format_ARGB32_Premultiplied);
		painter.begin(&image);
		painter.setRenderHint(QPainter::Antialiasing);
	}

	void Painter::render(const std::string& label)
	{
		// save PNG
		std::string filename(BWTA_PATH);
		if (label.empty()) {
			filename += MapData::mapFileName + "-" + std::to_string(renderCounter) + ".png";
			renderCounter++;
		} else {
			filename += MapData::mapFileName + "-" + label + ".png";
		}

		image.save(filename.c_str(), "PNG");

		// restart device
		painter.end();
		image = QImage(_width, _height, QImage::Format_ARGB32_Premultiplied);
		painter.begin(&image);
		painter.setRenderHint(QPainter::Antialiasing);
	}

	void Painter::drawMapBorder(uint16_t width, uint16_t height) 
	{
		QPen qp(Qt::black);
		qp.setWidth(2);
		painter.setPen(qp);
		painter.drawLine(0, 0, 0, height);
		painter.drawLine(0, height, width, height);
		painter.drawLine(width, height, width, 0);
		painter.drawLine(width, 0, 0, 0);
	}

	double Painter::getScale(Scale fromScale, Scale toScale)
	{
		if (fromScale == toScale) return 1.0;
		else if (fromScale == Scale::Pixel) {
			if (toScale == Scale::Walk) return 0.125;
			else return 0.03125;
		} else if (fromScale == Scale::Walk) {
			if (toScale == Scale::Pixel) return 8.0;
			else return 0.25;
		} else { // from Build Tile
			if (toScale == Scale::Walk) return 4.0;
			else return 32.0;
		}
	}

	void Painter::drawPolygon(const Polygon& polygon, QColor fillColor, Scale fromScale, Scale toScale)
	{
		double scale = getScale(fromScale, toScale);
		drawPolygon(polygon, fillColor, scale);
	}

	void Painter::drawPolygon(const Polygon& polygon, QColor fillColor, double scale) 
	{
		QVector<QPointF> qp;
		qp.reserve(polygon.size());
		for (const auto &point : polygon) {
			qp.push_back(QPointF(point.x * scale, point.y * scale));
		}

		painter.setPen(QPen(Qt::black));
		painter.setBrush(QBrush(fillColor));
		painter.drawPolygon(QPolygonF(qp));
	}

	void Painter::drawPolygons(const std::vector<Polygon*>& polygons, Scale fromScale, Scale toScale) 
	{
		for (const auto& polygon : polygons) {
			drawPolygon(*polygon, QColor(180, 180, 180), fromScale, toScale);
			for (const auto& hole : polygon->getHoles()) {
				drawPolygon(*hole, QColor(255, 100, 255), fromScale, toScale);
			}
		}
	}

	void Painter::drawRegions(const std::vector<Region*>& regions, Scale fromScale, Scale toScale) 
	{
		static bool colored = false;
		if (!colored) {
//			regionColoring();
			regionColoringHUE();
			colored = true;
		}
		for (const auto& r : regions) {
//			drawPolygon(r->getPolygon(), mapColors.at(r->getColorLabel()), fromScale, toScale);
			drawPolygon(r->getPolygon(), hsl2rgb(r->getHUE(), 1.0, 0.75), fromScale, toScale);
		}
	}

	void Painter::drawCoverPoints(const std::vector<Region*>& regions, Scale toScale) 
	{
		int size = 6;
		int middle = size / 2;
		Scale fromScale = Painter::Scale::Walk;
		double scale = getScale(fromScale, toScale);

		painter.setPen(QPen(Qt::blue));
		painter.setBrush(QBrush(Qt::blue));
		
		for (const auto& r : regions) {
			for (const auto& p : r->getCoverPoints()) {
				painter.drawEllipse((p.x - middle)*scale, (p.y - middle)*scale, size*scale, size*scale);
			}
		}

		painter.setPen(QPen(Qt::red));
		painter.setBrush(QBrush(Qt::red));
		for (const auto& r : regions) {
			BWAPI::WalkPosition p(r->getOpennessPosition());
			painter.drawEllipse((p.x - middle)*scale, (p.y - middle)*scale, size*scale, size*scale);
		}
	}

	QColor Painter::hsl2rgb(double h, double sl, double l)
	{
		double v;
		// default to gray
		double r = 1, g = 1, b = 1;
		v = (l <= 0.5) ? (l * (1.0 + sl)) : (l + sl - l * sl);
		if (v > 0) {
			double m;
			double sv;
			int sextant;
			double fract, vsf, mid1, mid2;
			m = l + l - v;
			sv = (v - m) / v;
			h *= 6.0;
			sextant = static_cast<int>(h);
			fract = h - sextant;
			vsf = v * sv * fract;
			mid1 = m + vsf;
			mid2 = v - vsf;
			switch (sextant) {
			case 0:
				r = v; g = mid1; b = m;
				break;
			case 1:
				r = mid2; g = v; b = m;
				break;
			case 2:
				r = m; g = v; b = mid1;
				break;
			case 3:
				r = m; g = mid2; b = v;
				break;
			case 4:
				r = mid1; g = m; b = v;
				break;
			case 5:
				r = v; g = m; b = mid2;
				break;
			default: break;
			}
		}
		return QColor(r*255.0, g*255.0, b*255.0);
	}

	void Painter::drawChokepoints(const std::set<Chokepoint*>& chokepoints, Scale toScale) {
		double lineScale = getScale(Scale::Walk, toScale);
		QPen qp(Qt::red);
		qp.setWidth(3 * lineScale);
		painter.setPen(qp);
		double scale = getScale(Scale::Pixel, toScale); // sides of chokepoints are stored at pixel resolution
		for (const auto& c : chokepoints) {
			const auto& sides = c->getSides();
			drawLine(sides.first.x, sides.first.y, sides.second.x, sides.second.y, scale);
		}
	}

	void Painter::drawLine(const int& x1, const int& y1, const int& x2, const int& y2, double scale)
	{
		painter.drawLine(x1 * scale, y1 * scale, x2 * scale, y2 * scale);
	}

	void Painter::getHeatMapColor(float value, int &red, int &green, int &blue) const
	{
		const int NUM_COLORS = 3;
		static float color[NUM_COLORS][3] = { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 } };
		// a static array of 3 colors:  (red, green, blue)

		int idx1;        // |-- our desired color will be between these two indexes in "color"
		int idx2;        // |
		float fractBetween = 0;  // fraction between "idx1" and "idx2" where our value is

		if (value <= 0)			{ idx1 = idx2 = 0; }				// accounts for an input <=0
		else if (value >= 1)	{ idx1 = idx2 = NUM_COLORS - 1; }	// accounts for an input >=0
		else {
			value = value * (NUM_COLORS - 1);	// will multiply value by 3
			idx1 = static_cast<int>(std::floor(value));	// our desired color will be after this index
			idx2 = idx1 + 1;					// ... and before this index (inclusive)
			fractBetween = value - float(idx1); // distance between the two indexes (0-1)
		}

		red   = static_cast<int>((color[idx2][0] - color[idx1][0])*fractBetween + color[idx1][0]);
		green = static_cast<int>((color[idx2][1] - color[idx1][1])*fractBetween + color[idx1][1]);
		blue  = static_cast<int>((color[idx2][2] - color[idx1][2])*fractBetween + color[idx1][2]);
	}

	void Painter::drawHeatMap(RectangleArray<int> map, float maxValue)
	{
		int red, green, blue;
		QColor heatColor;
		for (unsigned int x = 0; x < map.getWidth(); ++x) {
			for (unsigned int y = 0; y < map.getHeight(); ++y) {
				float normalized = static_cast<float>(map[x][y]) / maxValue;
				getHeatMapColor(normalized, red, green, blue);
				heatColor = QColor(red, green, blue);
				painter.setPen(QPen(heatColor));
				painter.setBrush(QBrush(heatColor));
				painter.drawEllipse(x, y, 1, 1);
			}
		}
	}

	void Painter::drawClosestBaseLocationMap(RectangleArray<BaseLocation*> map, std::set<BaseLocation*> baseLocations)
	{
		LOG("Drawing closest BaseLocation for " << baseLocations.size() << " bases");
		// assign a color to each BaseLocation
		std::vector<QColor> baseColors = { QColor(0, 114, 189), QColor(217, 83, 25), QColor(237, 177, 32)
			, QColor(126, 47, 142), QColor(119, 172, 48), QColor(77, 190, 238), QColor(162, 20, 47) };

		std::map<BaseLocation*, QColor> baseToColor;
		baseToColor[nullptr] = QColor(180, 180, 180);
		int i = 0;
		for (const auto& baseLocation : baseLocations) {
			i = i % baseColors.size();
			baseToColor[baseLocation] = baseColors.at(i);
			i++;
		}

		// draw BaseLocation closest map
		for (unsigned int x = 0; x < map.getWidth(); ++x) {
			for (unsigned int y = 0; y < map.getHeight(); ++y) {
				painter.setPen(QPen(baseToColor[map[x][y]]));
				painter.setBrush(QBrush(baseToColor[map[x][y]]));
				painter.drawEllipse(x, y, 1, 1);
			}
		}

		// draw BaseLocation origin
		QColor color(0, 0, 0);
		painter.setPen(QPen(color));
		painter.setBrush(QBrush(color));
		for (const auto& base : baseLocations) {
			painter.drawEllipse(base->getTilePosition().x * 4 - 6, base->getTilePosition().y * 4 - 6, 12, 12);
		}
	}

	void Painter::drawClosestChokepointMap(RectangleArray<Chokepoint*> map, std::set<Chokepoint*> chokepoints)
	{
		LOG("Drawing closest Chokepoint for " << chokepoints.size() << " chokepoints");
		// assign a color to each Chokepoint
		std::map<Chokepoint*, QColor> chokeToColor;
		chokeToColor[nullptr] = QColor(180, 180, 180);
		int i = 0;
		for (const auto& chokepoint : chokepoints) {
			i = i % baseColors.size();
			chokeToColor[chokepoint] = baseColors.at(i);
			i++;
		}

		// draw Chokepoint closest map
		for (unsigned int x = 0; x < map.getWidth(); ++x) {
			for (unsigned int y = 0; y < map.getHeight(); ++y) {
				painter.setPen(QPen(chokeToColor[map[x][y]]));
				painter.setBrush(QBrush(chokeToColor[map[x][y]]));
				painter.drawEllipse(x, y, 1, 1);
			}
		}

		// draw Chokepoint origin
		QColor color(0, 0, 0);
		painter.setPen(QPen(color));
		painter.setBrush(QBrush(color));
		for (const auto& chokepoint : chokepoints) {
			painter.drawEllipse(chokepoint->getCenter().x / 8, chokepoint->getCenter().y / 8, 12, 12);
		}
	}

	void Painter::drawEdges(std::vector<boost::polygon::voronoi_edge<double>> edges)
	{
		for (auto it = edges.begin(); it != edges.end(); ++it) {
			if (!it->is_primary()) {
				continue;
			}
			if (it->color() == 1) {
				QPen qp(QColor(255, 0, 0));
				qp.setWidth(2);
				painter.setPen(qp);
			} else {
				QPen qp(QColor(0, 0, 255));
				qp.setWidth(2);
				painter.setPen(qp);
			}
			if (!it->is_finite()) {
// 				clip_infinite_edge(*it, &samples);
			} else {
				painter.drawLine(it->vertex0()->x(), it->vertex0()->y(), 
					it->vertex1()->x(), it->vertex1()->y());
// 				if (it->is_curved()) {
// 					sample_curved_edge(*it, &samples);
// 				}
			}
		}
	}

	void Painter::drawGraph(const RegionGraph& graph, Scale fromScale, Scale toScale)
	{
		double scale = getScale(fromScale, toScale);
		QPen qp(Qt::blue);
		qp.setWidth(2 * scale);
		painter.setPen(qp);

		// container to mark visited nodes
		std::vector<bool> visited;
		visited.resize(graph.nodes.size());

		std::queue<nodeID> nodeToPrint;
		// find first node with children
		for (size_t id = 0; id < graph.adjacencyList.size(); ++id) {
			if (!graph.adjacencyList.at(id).empty()) {
				nodeToPrint.push(id);
				visited.at(id) = true;
			}
		}
		

		while (!nodeToPrint.empty()) {
			// pop first element
			nodeID v0 = nodeToPrint.front();
			nodeToPrint.pop();

			// draw point if it is an leaf node
// 			if (graph.adjacencyList.at(v0).size() == 1) {
// 				painter.drawEllipse(graph.nodes.at(v0).x - 6, graph.nodes.at(v0).y - 6, 12, 12);
// 				nodeID v1 = *graph.adjacencyList.at(v0).begin();
// 				LOG("Leaf dist: " << graph.minDistToObstacle.at(v0) << " - parent: " << graph.minDistToObstacle.at(v1));
// 			}

			// draw all edges of node
			for (const auto& v1 : graph.adjacencyList.at(v0)) {
				drawLine(graph.nodes.at(v0).x, graph.nodes.at(v0).y, graph.nodes.at(v1).x, graph.nodes.at(v1).y, scale);

				if (!visited.at(v1)) {
					nodeToPrint.push(v1);
					visited.at(v1) = true;
				}
			}
		}
	}

	void Painter::drawNodes(const RegionGraph& graph, const std::set<nodeID>& nodes, QColor color, Scale fromScale, Scale toScale, int size) 
	{
		double scale = getScale(fromScale, toScale);
		painter.setPen(QPen(color));
		painter.setBrush(QBrush(color));
		int middle = size / 2;
		for (const auto& v0 : nodes) {
			painter.drawEllipse((graph.nodes.at(v0).x - middle)*scale, (graph.nodes.at(v0).y - middle)*scale, size*scale, size*scale);
		}
	}

	void Painter::drawChokepointsSides(const std::map<nodeID, chokeSides_t>& chokepointSides, QColor color, Scale fromScale, Scale toScale)
	{
		double scale = getScale(fromScale, toScale);
		painter.setPen(QPen(color));
		painter.setBrush(QBrush(color));
		for (const auto& chokeSides : chokepointSides) {
			drawLine(chokeSides.second.side1.x, chokeSides.second.side1.y, 
					 chokeSides.second.side2.x, chokeSides.second.side2.y,
					 scale);
		}
	}

	void Painter::drawText(int x, int y, std::string text) {
		painter.setFont(QFont("Tahoma", 8, QFont::Bold));
		painter.setPen(QPen(Qt::darkGreen));
		QRect rect = image.rect();
		rect.setLeft(5);
		painter.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, QString::fromStdString(text));
	}

	void Painter::drawBaseLocations(const std::set<BaseLocation*>& baseLocations, Scale toScale) {
		Scale fromScale = Scale::Build;
		double scale = getScale(fromScale, toScale);
		int baseWidth = 4*scale;
		int baseHeight = 3*scale;
		int mineralWidth = 2*scale;
		int mineralHeight = 1*scale;
		int vespeneWidth = 4*scale;
		int vespeneHeight = 2*scale;

		QPen qp(Qt::blue);
		int penWidth = std::max(1, static_cast<int>(scale/4));
		qp.setWidth(penWidth);
		painter.setPen(qp);
		painter.setBrush(Qt::NoBrush);

		for (const auto& base : baseLocations) {
			int x = base->getTilePosition().x * scale;
			int y = base->getTilePosition().y * scale;
			if (base->isStartLocation()) painter.fillRect(x, y, baseWidth, baseHeight, Qt::red);
			painter.drawRect(x, y, baseWidth, baseHeight);
			BaseLocationImpl* b = static_cast<BaseLocationImpl*>(base);
			for (const auto& r : b->resources) {
				if (r.type == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
					painter.drawLine(x + (baseWidth/2), y + (baseHeight/2), r.pos.x*scale + vespeneWidth/2, r.pos.y*scale + vespeneHeight/2);
					painter.fillRect(r.pos.x * scale, r.pos.y * scale, vespeneWidth, vespeneHeight, Qt::green);
				} else {
					if (r.isBlocking) {
						qp.setColor(Qt::red);
						painter.setPen(qp);
					}
					painter.drawLine(x + (baseWidth/2), y + (baseHeight/2), r.pos.x*scale + mineralWidth/2, r.pos.y*scale + mineralHeight/2);
					painter.fillRect(r.pos.x * scale, r.pos.y * scale, mineralWidth, mineralHeight, Qt::cyan);
					if (r.isBlocking) {
						qp.setColor(Qt::blue);
						painter.setPen(qp);
					}
				}
				
			}
		}
	}
}
#endif