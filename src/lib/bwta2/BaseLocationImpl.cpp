#include "BaseLocationImpl.h"

namespace BWTA
{
	BaseLocationImpl::BaseLocationImpl(const BWAPI::TilePosition &tp, std::vector<resource_t> r)
		: region(nullptr)
		, _isIsland(true)	// by default isIsland unless we can walk from this BaseLocation to another BaseLocation
		, _isStartLocation(false)
		, tilePosition(tp)
		, position(tp.x * BWAPI::TILEPOSITION_SCALE + 64, tp.y * BWAPI::TILEPOSITION_SCALE + 48)
		, resources(r)
	{
	}

	const int BaseLocationImpl::minerals() const
	{
		int count = 0;
		for (const auto& m : this->staticMinerals) count += m->getResources();
		return count;
	}

	const int BaseLocationImpl::gas() const
	{
		int count = 0;
		for (const auto& g : this->geysers) count += g->getResources();
		return count;
	}

	const BWAPI::Unitset& BaseLocationImpl::getMinerals()
	{
		//  check if a mineral have been deleted (or we don't have access to them)
		currentMinerals.clear();
		for (auto mineral : staticMinerals) {
			if (mineral->exists()) currentMinerals.insert(mineral);
		}
		return currentMinerals;
	}

	const double BaseLocationImpl::getGroundDistance(BaseLocation* other) const
	{
		auto it = groundDistances.find(other);
		if (it == groundDistances.end()) return -1;
		return (*it).second;
	}

	const double BaseLocationImpl::getAirDistance(BaseLocation* other) const
	{
		auto it = airDistances.find(other);
		if (it == airDistances.end()) return -1;
		return (*it).second;
	}

	void BaseLocationImpl::setTile(BWAPI::TilePosition tilePos)
	{
		tilePosition = tilePos;
		position = BWAPI::Position(tilePosition);
	}
}