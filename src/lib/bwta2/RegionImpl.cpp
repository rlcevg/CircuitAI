#include "RegionImpl.h"
namespace BWTA
{
  RegionImpl::RegionImpl(){} // TODO remove after fixing load_data

  RegionImpl::RegionImpl(const Polygon& poly)
	  : _polygon(poly),
	  _maxDistance(0),
	  _color(0),
	  _hue(0.0)
  {
	  // TODO review this (wrong position)
	  this->_center = _polygon.getCenter();
  }

  RegionImpl::RegionImpl(const BoostPolygon& boostPoly, const int& scale)
	  : _polygon(PolygonImpl(boostPoly, scale)),
	  _maxDistance(0),
	  _color(0),
	  _hue(0.0)
  {
	  // TODO review this (wrong position)
	  this->_center = _polygon.getCenter();
  }

  bool RegionImpl::isReachable(Region* region) const
  {
    return this->reachableRegions.find(region)!=this->reachableRegions.end();
  }

}