#pragma once
#include <BWTA/Polygon.h>
#include <BWTA/Region.h>
#include <BWTA/BaseLocation.h>
#include "PolygonImpl.h"

namespace BWTA
{
  class Chokepoint;
  class RegionImpl : public Region
  {
  public:
    RegionImpl();
	RegionImpl(const Polygon& poly);
	RegionImpl(const BoostPolygon& boostPoly, const int& scale = 1);

	const Polygon& getPolygon() const override							{ return _polygon; }
//	const BWAPI::Position& getCenter() const override					{ return this->_center; }
	const BWAPI::Position& getCenter() const override					{ return this->_opennessPoint; }
	const std::set<Chokepoint*>& getChokepoints() const override		{ return this->_chokepoints; }
	const std::set<BaseLocation*>& getBaseLocations() const override	{ return this->baseLocations; }
	const std::set<Region*>& getReachableRegions() const override		{ return this->reachableRegions; }
	const int getMaxDistance() const override							{ return this->_maxDistance; }
	const int getColorLabel() const override							{ return this->_color; }
	const double getHUE() const override								{ return this->_hue; }
	const int getLabel() const override									{ return this->_label; }
	const BWAPI::Position& getOpennessPosition() const override			{ return this->_opennessPoint; }
	const double getOpennessDistance() const override					{ return this->_opennessDistance; }
	const std::vector<BWAPI::WalkPosition>& getCoverPoints() const override { return _coveragePositions; }

	bool isReachable(Region* region) const override;

    PolygonImpl _polygon;
    BWAPI::Position _center;
    std::set<Chokepoint*> _chokepoints;
    std::set<BaseLocation*> baseLocations;
    std::set<Region*> reachableRegions;
	int _maxDistance; // TODO remove this, should be the same as _opennessDistance
	int _color;						// color map ID
	double _hue;
	int _label;						// region label id from BWTA_Result::regionLabelMap
	BWAPI::Position _opennessPoint; // maximum distance point equidistant to the border
	double _opennessDistance;		// distance from opennessPoint to the border
// 	int _connectedComponentId; // TODO
	std::vector<BWAPI::WalkPosition> _coveragePositions;
  };
}