#pragma once

#include <BWTA/BaseLocation.h>
//#include "../../OfflineExtractor/MapFileParser.h"
#include "terrain/bwta2/MapData.h"

namespace BWTA
{
	class BaseLocationImpl : public BaseLocation
	{
	public:
		BaseLocationImpl() {}
		BaseLocationImpl(const BWAPI::TilePosition &tp, std::vector<resource_t> resources);

		const BWAPI::Position getPosition() const override { return position; }
		const BWAPI::TilePosition getTilePosition() const override { return tilePosition; }

		Region* getRegion() const override { return region; }

		const int minerals() const override;
		const int gas() const override;

		const BWAPI::Unitset &getMinerals() override;
		const BWAPI::Unitset &getStaticMinerals() const override { return staticMinerals; }
		const BWAPI::Unitset &getGeysers() const override { return geysers; }

		const double getGroundDistance(BaseLocation* other) const override;
		const double getAirDistance(BaseLocation* other) const override;

		const bool isIsland() const override { return _isIsland; }
		const bool isMineralOnly() const override { return geysers.empty(); }
		const bool isStartLocation() const override { return _isStartLocation; }

		void setTile(BWAPI::TilePosition tilePos);

		//-----------------------------------------------------------

		Region* region;
		bool _isIsland;
		bool _isStartLocation;
		BWAPI::TilePosition tilePosition;
		BWAPI::Position position;
		BWAPI::Unitset geysers;
		BWAPI::Unitset staticMinerals;
		BWAPI::Unitset currentMinerals;
		std::map<BaseLocation*, double> groundDistances;
		std::map<BaseLocation*, double> airDistances;
		std::vector<resource_t> resources;
	};
}
