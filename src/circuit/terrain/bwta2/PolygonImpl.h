#pragma once
#include <BWTA/Polygon.h>

#include "stdafx.h"

namespace BWTA 
{
	class PolygonImpl : public Polygon
	{
	public:
		PolygonImpl(); // TODO remove after fixing load_data
		PolygonImpl(const BoostPolygon& boostPol, const int& scale = 1);
		PolygonImpl(const Polygon& b);

		const double getArea() const override;
		const double getPerimeter() const override;
		const BWAPI::Position getCenter() const override;
		BWAPI::Position getNearestPoint(const BWAPI::Position &p) const override;
 		const std::vector<Polygon*>& getHoles() const override { return holes; };
		void addHole(const PolygonImpl &h);
	};
}