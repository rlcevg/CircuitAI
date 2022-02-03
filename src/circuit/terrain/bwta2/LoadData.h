#pragma once

#include "Utils.h"
#include "MapData.h"
#include "BWTA_Result.h"
#include "BaseLocationImpl.h"
#include "ChokepointImpl.h"
#include "RegionImpl.h"
#include "PolygonImpl.h"

namespace BWTA
{
  void loadMapFromBWAPI();
  void loadMap();
  void load_data(std::string filename);
  void save_data(std::string filename);
}