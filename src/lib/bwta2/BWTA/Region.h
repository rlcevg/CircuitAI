#pragma once

#include <set>

#include <BWAPI.h>

namespace BWTA
{
	class Chokepoint;
	class BaseLocation;
	class Region
	{
	public:
		virtual ~Region() {};
		virtual const Polygon& getPolygon() const = 0;
		virtual const BWAPI::Position& getCenter() const = 0;
		virtual const std::set<Chokepoint*>& getChokepoints() const = 0;
		virtual const std::set<BaseLocation*>& getBaseLocations() const = 0;
		virtual bool isReachable(Region* region) const = 0;
		virtual const std::set<Region*>& getReachableRegions() const = 0;
		virtual const int getMaxDistance() const = 0;
		virtual const int getColorLabel() const = 0;
		virtual const double getHUE() const = 0;
		virtual const int getLabel() const = 0;
		virtual const BWAPI::Position& getOpennessPosition() const = 0;
		virtual const double getOpennessDistance() const = 0;
		virtual const std::vector<BWAPI::WalkPosition>& getCoverPoints() const = 0;
	};
}