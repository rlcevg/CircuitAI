#include "GraphColoring.h"

namespace BWTA
{

	// this function finds the unprocessed vertex of which degree is maximum
	int MaxDegreeVertex(const std::vector<int>& color)
	{
		size_t max = 0;
		int maxIndex = 0;
		for (size_t i = 0; i < color.size(); ++i) {
			if (color[i] == 0) {
				int degree = BWTA_Result::regions.at(i)->getChokepoints().size();
				if (BWTA_Result::regions.at(i)->getChokepoints().size() > max) {
					max = degree;
					maxIndex = i;
				}
			}
		}
		return maxIndex;
	}

	bool isAdjacent(const size_t& i, const size_t& j) {
		Region* reg1 = BWTA_Result::regions.at(i);
		Region* reg2 = BWTA_Result::regions.at(j);
		for (const auto& c : reg1->getChokepoints()) {
			if (c->getRegions().first == reg2 || c->getRegions().second == reg2) return true;
		}
		return false;
	}

	void UpdateNN(const std::vector<int>& color, const int& colorNumber, std::vector<size_t>& NN, size_t& NNSzie)
	{
		NNSzie = 0;
		// firstly, add all the uncolored vertices into NN set
		for (size_t i = 0; i < color.size(); ++i) {
			if (color[i] == 0) {
				NN[NNSzie] = i;
				NNSzie++; // when we add a vertex, increase the NNSzie
			}
		}

		// then, remove all the vertices in NN that
		// is adjacent to the vertices colored ColorNumber
		for (size_t i = 0; i < color.size(); ++i) {
			// find one vertex colored ColorNumber
			if (color[i] == colorNumber) {
				for (size_t j = 0; j < NNSzie; ++j) {
					while (isAdjacent(i, NN[j])) { // remove vertex that adjacent to the found vertex
						NN[j] = NN[NNSzie - 1];
						NNSzie--; // decrease the NNCount
						if (NNSzie == 0) break;
					}
				}
			}
		}
	}

	// this function will find suitable y from NN
	int findSuitableY(const std::vector<int>& color, int colorNumber, int& verticesInCommon, std::vector<size_t>& NN, size_t& NNSzie)
	{
		int temp, tmp_y, y = 0;
		// array scanned stores uncolored vertices
		// except the vertex is being processing
		std::vector<bool> scanned; scanned.resize(color.size());
		verticesInCommon = 0;
		for (size_t i = 0; i < NNSzie; i++) { // check the i-th vertex in NN
			tmp_y = NN[i]; // the vertex we are processing
			// temp is the neighbors in common of tmp_y
			// and the vertices colored ColorNumber
			temp = 0;
                        for (auto it = scanned.begin(); it != scanned.end(); ++it) {
                            // reset scanned values in order to check all 
                            *it = false;
                        }
			//the vertices if they are adjacent to i-th vertex in NN
			for (size_t x = 0; x < color.size(); ++x) {
				if (color[x] == colorNumber) { // find one vertex colored ColorNumber
					for (size_t k = 0; k < color.size(); ++k) {
						if (color[k] == 0 && scanned[k] == 0) {
							if (isAdjacent(x, k) && isAdjacent(tmp_y, k)) {
								temp++;
								scanned[k] = true; // k is scanned
							}
						}
					}
				}
			}
			if (temp > verticesInCommon) {
				verticesInCommon = temp;
				y = tmp_y;
			}
		}
		return y;
	}

	int MaxDegreeInNN(const std::vector<int>& color, std::vector<size_t>& NN, size_t& NNSzie)
	{
		int tmp_y = NN[0]; // the vertex has the current maximum degree
		int temp, max = 0;
		for (size_t i = 0; i < NNSzie; ++i) {
			temp = 0;
			for (size_t j = 0; j < color.size(); ++j) {
				if (color[j] == 0 && isAdjacent(NN[i], j)) temp++;
			}
			if (temp > max) { // if the degree of vertex NN[i] is higher than tmp_y's one
				max = temp; // assignment NN[i] to tmp_y
				tmp_y = NN[i];
			}
		}
		return tmp_y;
	}

	void regionColoring()
	{
		size_t numRegions = BWTA_Result::regions.size();
		std::vector<int> color; color.resize(numRegions); // stores color of the vertices (0 if not colored it yet)
		std::vector<size_t> NN; NN.resize(numRegions); // stores all the vertices that is not adjacent to current vertex
		size_t NNSzie; // current size
		size_t unprocessed = numRegions; //  number of vertices with which we have not worked

		int x, y;
		int colorNumber = 0;
		int verticesInCommon = 0;
		while (unprocessed > 0) { // while there is an uncolored vertex

			x = MaxDegreeVertex(color); // find the one with maximum degree
			colorNumber++;
			color[x] = colorNumber; // give it a new color
			unprocessed--;
			UpdateNN(color, colorNumber, NN, NNSzie); // find the set of non-neighbors of x

			while (NNSzie > 0) {
				// find y, the vertex has the maximum neighbors in common with x
				// VerticesInCommon is this maximum number
				y = findSuitableY(color, colorNumber, verticesInCommon, NN, NNSzie);
				// in case VerticesInCommon = 0
				// y is determined that the vertex with max degree in NN
				if (verticesInCommon == 0)
					y = MaxDegreeInNN(color, NN, NNSzie);
				// color y the same to x
				color[y] = colorNumber;
				unprocessed--;
				UpdateNN(color, colorNumber, NN, NNSzie); // find the new set of non-neighbors of x
			}
		}

		// save each color to each region
		for (size_t i = 0; i < BWTA_Result::regions.size(); ++i) {
			RegionImpl* reg = dynamic_cast<RegionImpl*>(BWTA_Result::regions.at(i));
			reg->_color = color.at(i);
		}
	}

	const std::vector<size_t> getAdjacents(const size_t& regId) {
		std::vector<size_t> adjacentList;
		for (size_t regId2 = 0; regId2 < BWTA_Result::regions.size(); ++regId2) {
			if (regId != regId2 && isAdjacent(regId, regId2)) {
				adjacentList.push_back(regId2);
			}
		}
		return adjacentList;
	}

	void regionColoringHUE() {
		size_t numRegions = BWTA_Result::regions.size();
		std::vector<double> hueList; hueList.resize(numRegions);
		// Assign a random HUE to each region
		for (auto& hue : hueList) hue = rand()*1.0 / RAND_MAX;
		// Check constrain satisfaction
		double d, s;
		for (int l = 0; l < 6; ++l) {
			for (size_t regId = 0; regId < numRegions; ++regId) {
				for (auto& neighborId : getAdjacents(regId)) {
					d = hueList.at(neighborId) - hueList.at(regId);
					if (d > 0.5) d = d - 1.0;
					if (d < -0.5) d = d + 1.0;
					s = d - 0.5;
					if (d < 0) s += 1.0;
					s *= 0.05;
					hueList.at(regId) += s;
					hueList.at(neighborId) -= s;
					while (hueList.at(regId) < 0) hueList.at(regId) += 1.0;
					while (hueList.at(regId) >= 1.0) hueList.at(regId) -= 1.0;
					while (hueList.at(neighborId) < 0) hueList.at(neighborId) += 1.0;
					while (hueList.at(neighborId) >= 1.0) hueList.at(neighborId) -= 1.0;
				}
			}
		}
		// save each color to each region
		for (size_t i = 0; i < BWTA_Result::regions.size(); ++i) {
			RegionImpl* reg = dynamic_cast<RegionImpl*>(BWTA_Result::regions.at(i));
			reg->_hue = hueList.at(i);
		}
	}

}
