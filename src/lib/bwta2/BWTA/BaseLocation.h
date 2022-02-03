#pragma once
#include <BWAPI.h>
namespace BWTA
{
	class Region;
	class BaseLocation
	{
	public:
		virtual const BWAPI::Position getPosition() const = 0;
		virtual const BWAPI::TilePosition getTilePosition() const = 0;

		virtual Region* getRegion() const = 0;

		virtual const int minerals() const = 0;
		virtual const int gas() const = 0;

		virtual const BWAPI::Unitset &getMinerals() = 0;
		virtual const BWAPI::Unitset &getStaticMinerals() const = 0;
		virtual const BWAPI::Unitset &getGeysers() const = 0;

		virtual const double getGroundDistance(BaseLocation* other) const = 0;
		virtual const double getAirDistance(BaseLocation* other) const = 0;

		virtual const bool isIsland() const = 0;
		virtual const bool isMineralOnly() const = 0;
		virtual const bool isStartLocation() const = 0;
	};
}