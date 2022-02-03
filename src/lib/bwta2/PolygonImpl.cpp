#include "PolygonImpl.h"

namespace BWTA
{

	PolygonImpl::PolygonImpl()  // TODO remove after fixing load_data
	{
// 	  LOG("Empty polygon construct called"); 
	}

	// scale is used if you want to change the scale of the positions of the original BoostPolygon
	// like WalkPosition to Position
	PolygonImpl::PolygonImpl(const BoostPolygon& boostPol, const int& scale)
	{
		for (const auto& polyPoint : boostPol.outer()) {
			this->emplace_back((int)polyPoint.x() * scale, (int)polyPoint.y() * scale);
		}
		// TODO add holes
	}

	PolygonImpl::PolygonImpl(const Polygon& b)
	{
		for (const auto& point : b) this->push_back(point);
		for (const auto& h : b.getHoles()) {
			holes.push_back(new PolygonImpl(*h));
		}
	}

	void PolygonImpl::addHole(const PolygonImpl &h)
	{
		holes.push_back(new PolygonImpl(h));
	}

	const double PolygonImpl::getArea() const
	{
		if (this->size() < 3) return 0;

		int a = this->back().x * this->front().y - this->front().x * this->back().y;;
		size_t lastPoint = this->size() - 1;
		for (size_t i = 0, j = 1; i < lastPoint; ++i, ++j) {
			a += this->at(i).x * this->at(j).y - this->at(j).x * this->at(i).y;
		}
		return std::fabs((double)a / 2.0);
	}

	const double PolygonImpl::getPerimeter() const
	{
		if (this->size() < 2) return 0;

		double p = this->back().getDistance(this->front());
		size_t lastPoint = this->size() - 1;
		for (size_t i = 0, j = 1; i < lastPoint; ++i, ++j) {
			p += this->at(i).getDistance(this->at(j));
		}
		return p;
	}

	const BWAPI::Position PolygonImpl::getCenter() const
	{
		int temp = this->back().x*this->front().y - this->front().x*this->back().y;
		int cx = (this->back().x + this->front().x) * temp;
		int cy = (this->back().y + this->front().y) * temp;

		size_t lastPoint = this->size() - 1;
		for (size_t i = 0, j = 1; i < lastPoint; ++i, ++j) {
			temp = this->at(i).x*this->at(j).y - this->at(j).x*this->at(i).y;
			cx += (this->at(i).x + this->at(j).x) * temp;
			cy += (this->at(i).y + this->at(j).y) * temp;
		}

		double area = getArea() * 6.0;
		cx = int((double)cx / area);
		cy = int((double)cy / area);
		return BWAPI::Position(cx, cy);
	}

	BWAPI::Position PolygonImpl::getNearestPoint(const BWAPI::Position &p) const
	{
		double x3 = p.x;
		double y3 = p.y;
		BWAPI::Position minp = BWAPI::Positions::Unknown;
		size_t j;
		double mind2 = -1;

		for (size_t i = 0; i < this->size(); ++i) {
			j = (i + 1) % this->size();
			double x1 = this->at(i).x;
			double y1 = this->at(i).y;
			double x2 = this->at(j).x;
			double y2 = this->at(j).y;
			double u = ((x3 - x1)*(x2 - x1) + (y3 - y1)*(y2 - y1)) / ((x2 - x1)*(x2 - x1) + (y2 - y1)*(y2 - y1));
			if (u < 0) u = 0;
			if (u > 1) u = 1;
			double x = x1 + u*(x2 - x1);
			double y = y1 + u*(y2 - y1);
			double d2 = (x - x3)*(x - x3) + (y - y3)*(y - y3);
			if (mind2 < 0 || d2 < mind2) {
				mind2 = d2;
				minp = BWAPI::Position((int)x, (int)y);
			}
		}

		for (const auto& hole : holes) {
			BWAPI::Position hnp = hole->getNearestPoint(p);
			if (hnp.getDistance(p) < minp.getDistance(p)) minp = hnp;
		}
		return minp;
	}
}