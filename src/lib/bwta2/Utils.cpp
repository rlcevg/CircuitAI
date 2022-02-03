#include "Utils.h"
#include "filesystem/path.h"

namespace BWTA
{
	void polygonBoundingBox(const Contour& contour, size_t& maxX, size_t& minX, size_t& maxY, size_t& minY)
	{
		maxX = 0;
		minX = std::numeric_limits<size_t>::max();
		maxY = 0;
		minY = std::numeric_limits<size_t>::max();
		for (const auto& pos : contour) {
			maxX = std::max(maxX, (size_t)pos.x());
			minX = std::min(minX, (size_t)pos.x());
			maxY = std::max(maxY, (size_t)pos.y());
			minY = std::min(minY, (size_t)pos.y());
		}
	}


	void scanLineFill(Contour contour, const int& labelID,
		RectangleArray<int>& labelMap, RectangleArray<bool>& nodeMap, bool fillContour) 
	{
		if (contour.size() < 2) return;
		// Detect nodes for scan-line fill algorithm (avoiding edges)
		contour.pop_back(); // since first and last point are the same
		BoostPoint left = contour.back();
		BoostPoint pos, right;
		size_t last = contour.size() - 1;
		for (size_t i = 0; i < last; ++i) {
			pos = contour.at(i);
			right = contour.at(i + 1);
			if ((left.y() <= pos.y() && right.y() <= pos.y()) ||
				(left.y() > pos.y() && right.y() > pos.y()))
			{ // we have an edge
// 				labelMap[(int)pos.x()][(int)pos.y()] = 9;
			} else { // we have a node
// 				labelMap[(int)pos.x()][(int)pos.y()] = 8;
				nodeMap[(int)pos.x()][(int)pos.y()] = true;
			}
			left = pos;
		}
		// check last element
		pos = contour.back();
		right = contour.front();
		if ((left.y() <= pos.y() && right.y() <= pos.y()) ||
			(left.y() > pos.y() && right.y() > pos.y()))
		{ // we have an edge
// 			labelMap[(int)pos.x()][(int)pos.y()] = 9;
		} else { // we have a node
// 			labelMap[(int)pos.x()][(int)pos.y()] = 8;
			nodeMap[(int)pos.x()][(int)pos.y()] = true;
		}

		// find bounding box of polygon
		size_t maxX, minX, maxY, minY;
		polygonBoundingBox(contour, maxX, minX, maxY, minY);

		// iterate though the bounding box using a scan-line algorithm to fill the polygon
		bool toFill;
		for (size_t posY = minY; posY < maxY; ++posY) {
			toFill = false;
			for (size_t posX = minX; posX < maxX; ++posX) {
				if (toFill) labelMap[posX][posY] = labelID;
				if (nodeMap[posX][posY]) toFill = !toFill;
			}
		}

		if (fillContour) { // we mark also the contour
			for (const auto& pos : contour) {
				labelMap[(int)pos.x()][(int)pos.y()] = labelID;
			}
		}
	}

	void scanLineFill(const Contour &polyCorners, const int& labelID, RectangleArray<int>& labelMap) 
	{
		// find bounding box of polygon
		size_t maxX, minX, maxY, minY;
		polygonBoundingBox(polyCorners, maxX, minX, maxY, minY);

		size_t nodes, nodeX[256]; // 256 is MAX_POLY_CORNERS
		size_t pixelX, pixelY, i, j, swap;

		//  Loop through the rows of the image.
		for (pixelY = minY; pixelY < maxY; ++pixelY) {

			//  Build a list of nodes.
			nodes = 0;
			j = 0;
			for (i = 1; i < polyCorners.size(); i++) {
				if (polyCorners.at(i).y() < (double)pixelY && polyCorners.at(j).y() >= (double)pixelY
					|| polyCorners.at(j).y() < (double)pixelY && polyCorners.at(i).y() >= (double)pixelY) {
					nodeX[nodes++] = size_t(polyCorners.at(i).x() + (pixelY - polyCorners.at(i).y()) /
						(polyCorners.at(j).y() - polyCorners.at(i).y()) * (polyCorners.at(j).x() - polyCorners.at(i).x()));
				}
				j = i;
			}

			//  Sort the nodes, via a simple “Bubble” sort.
			if (nodes > 0) {
				i = 0;
				while (i < nodes - 1) {
					if (nodeX[i] > nodeX[i + 1]) {
						swap = nodeX[i]; nodeX[i] = nodeX[i + 1]; nodeX[i + 1] = swap; if (i) i--;
					} else {
						i++;
					}
				}
			}

			//  Fill the pixels between node pairs.
			for (i = 0; i < nodes; i += 2) {
				if (nodeX[i] >= maxX) break;
				if (nodeX[i + 1] > minX) {
					if (nodeX[i] < minX) nodeX[i] = minX;
					if (nodeX[i + 1] > maxX) nodeX[i + 1] = maxX;
					for (pixelX = nodeX[i]; pixelX < nodeX[i + 1]; pixelX++) labelMap[pixelX][pixelY] = labelID;
				}
			}
		}
	}

	bool isFileVersionCorrect(std::string filename)
	{
		filesystem::path filePath(filesystem::path::get_cwd() / filename);
		if (filePath.exists()) {
			// get file version
			std::ifstream file_in;
			file_in.open(filename.c_str());
			int version;
			file_in >> version;
			file_in.close();

			// return comparison
			return version == BWTA_FILE_VERSION;
		}
		return false;
	}

}