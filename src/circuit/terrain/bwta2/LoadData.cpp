#include "LoadData.h"

using namespace BWAPI;
namespace BWTA
{
	struct square_t {
		size_t minX;
		size_t maxX;
		size_t minY;
		size_t maxY;
		square_t(size_t _minX, size_t _maxX, size_t _minY, size_t _maxY) :
			maxX(_maxX), minX(_minX), maxY(_maxY), minY(_minY) {}
	};

	void loadMapFromBWAPI()
	{
		// load map name
		MapData::hash = BWAPI::Broodwar->mapHash();
		MapData::mapFileName = BWAPI::Broodwar->mapFileName();
		// Clean previous log file
		std::ofstream logFile(LOG_FILE_PATH);
		logFile << "Map name: " << MapData::mapFileName << std::endl;

		// load map info
		MapData::mapWidthTileRes = BWAPI::Broodwar->mapWidth();
		MapData::mapWidthPixelRes = MapData::mapWidthTileRes * TILE_SIZE;
		MapData::mapWidthWalkRes = MapData::mapWidthTileRes * 4;

		MapData::mapHeightTileRes = BWAPI::Broodwar->mapHeight();
		MapData::mapHeightPixelRes = MapData::mapHeightTileRes * TILE_SIZE;
		MapData::mapHeightWalkRes = MapData::mapHeightTileRes * 4;

		MapData::buildability.resize(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		for (int x = 0; x < MapData::mapWidthTileRes; x++) {
			for (int y = 0; y < MapData::mapHeightTileRes; y++) {
				MapData::buildability[x][y] = BWAPI::Broodwar->isBuildable(x, y);
			}
		}

		MapData::rawWalkability.resize(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		for (int x = 0; x < MapData::mapWidthWalkRes; x++) {
			for (int y = 0; y < MapData::mapHeightWalkRes; y++) {
				MapData::rawWalkability[x][y] = BWAPI::Broodwar->isWalkable(x, y);
			}
		}

		// load static buildings
		MapData::staticNeutralBuildings.clear();
		for (auto unit : BWAPI::Broodwar->getStaticNeutralUnits()) {
			auto unitType = unit->getType();
			// checks if it is a resource container
			if (unitType == BWAPI::UnitTypes::Resource_Vespene_Geyser || unitType.isMineralField()) continue;

//			LOG("Doodad " << unitType << " at " << unit->getPosition() << " can move? " << unitType.canMove() <<
//				" is invincible? " << unitType.isInvincible());
            // Ignores also the various creature types that can move around
			// WARNING unitType.isInvincible() returns FALSE with some doodas like Special_Power_Generator
            if (unitType.canMove())  continue;

			MapData::staticNeutralBuildings.emplace_back(unitType, unit->getPosition());
		}

		// load resources (minerals, gas) and start locations
//		MapData::resourcesWalkPositions.clear();
		MapData::resources.clear();
		for (auto mineral : BWAPI::Broodwar->getStaticMinerals()) {
//			if (mineral->getInitialResources() > 200) { //filter out all mineral patches under 200
//				BWAPI::WalkPosition unitWalkPosition(mineral->getPosition());
//				MapData::resourcesWalkPositions.push_back(std::make_pair(mineral->getType(), unitWalkPosition));
				MapData::resources.emplace_back(mineral->getType(), mineral->getTilePosition(),mineral->getInitialResources());
//			}
		}

		for (auto geyser : BWAPI::Broodwar->getStaticGeysers()) {
			BWAPI::WalkPosition unitWalkPosition(geyser->getPosition());
//			MapData::resourcesWalkPositions.push_back(std::make_pair(geyser->getType(), unitWalkPosition));
			MapData::resources.emplace_back(geyser->getType(), geyser->getTilePosition(), geyser->getInitialResources());
		}

		MapData::startLocations = BWAPI::Broodwar->getStartLocations();
	}

	void loadMap()
	{
		// init distance transform map
		MapData::distanceTransform.resize(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		for (int x = 0; x < MapData::mapWidthWalkRes; x++) {
			for (int y = 0; y < MapData::mapHeightWalkRes; y++) {
				if (MapData::rawWalkability[x][y]) {
					if (x == 0 || x == MapData::mapWidthWalkRes - 1 || y == 0 || y == MapData::mapHeightWalkRes - 1){
						MapData::distanceTransform[x][y] = 1;
					} else {
						MapData::distanceTransform[x][y] = -1;
					}
				} else {
					MapData::distanceTransform[x][y] = 0;
				}
			}
		}

		// init walkability map and lowResWalkability map
		MapData::lowResWalkability.resize(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		MapData::lowResWalkability.setTo(true);

		MapData::walkability.resize(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		MapData::walkability.setTo(true);

		size_t maxWidth2 = MapData::mapWidthWalkRes - 2;
		size_t maxHeight2 = MapData::mapHeightWalkRes - 2;
		size_t maxWidth1 = MapData::mapWidthWalkRes - 1;
		size_t maxHeight1 = MapData::mapHeightWalkRes - 1;

		for (size_t x = 0; x < MapData::mapWidthWalkRes; ++x) {
			for (size_t y = 0; y < MapData::mapHeightWalkRes; ++y) {
				// the smallest unit size is 16x16 pixels (Zerglings), that is 2x2 walk tiles
				// to be safe, we check if there is room for a generic small unit (3x3 walk tiles) in the 4 corners
				std::vector<square_t> boxes;
				// top left corner
				if (x >= 2 && y >= 2) boxes.emplace_back(x - 2, x, y - 2, y);
				// top right corner
				if (x < maxWidth2 && y >= 2) boxes.emplace_back(x, x + 2, y - 2, y);
				// bottom left corner
				if (x >= 2 && y < maxHeight2) boxes.emplace_back(x - 2, x, y, y + 2);
				// bottom right corner
				if (x < maxWidth2 && y < maxHeight2) boxes.emplace_back(x, x + 2, y, y + 2);
				// middle
				if (x >= 1 && y >= 1 && x < maxWidth1 && y < maxHeight1) {
					boxes.emplace_back(x - 1, x + 1, y - 1, y + 1);
				}
				bool cornerWalkable = true;
				for (const auto& box : boxes) {
					cornerWalkable = true;
					for (size_t x2 = box.minX; x2 <= box.maxX; ++x2) {
						for (size_t y2 = box.minY; y2 <= box.maxY; ++y2) {
							cornerWalkable &= MapData::rawWalkability[x2][y2];
						}
					}
					if (cornerWalkable) break;
				}
				MapData::walkability[x][y] = cornerWalkable;

				// make thin unwalkable areas bigger
				if (!MapData::walkability[x][y] && x >= 2 && x < maxWidth2) {
					int unwalkTiles = 0;
					size_t wideX = x + 2;
					for (size_t x2 = x - 2; x2 <= wideX; ++x2) {
						if (!MapData::rawWalkability[x2][y]) unwalkTiles++;
					}
					if (unwalkTiles <= 2) {
// 						MapData::walkability[x - 2][y] = false;
						MapData::walkability[x - 1][y] = false;
					}
				}

				// the lowResWalkability has built tile resolution
				// so a built tile is walkable only if all 4x4 tiles are walkable
				MapData::lowResWalkability[x / 4][y / 4] &= MapData::rawWalkability[x][y];
			}
		}

		// smooth borders
		for (size_t x = 1; x < maxWidth1; ++x) {
			if (!MapData::walkability[x][0]) {
				MapData::walkability[x][3] = false;
				MapData::walkability[x][2] = false;
				MapData::walkability[x][1] = false;
			} else {
				MapData::walkability[x][2] &= MapData::walkability[x][3];
				MapData::walkability[x][1] &= MapData::walkability[x][2];
				MapData::walkability[x][0] &= MapData::walkability[x][1];
			}
		}
		for (size_t x = 1; x < maxWidth2; ++x) {
			if (!MapData::walkability[x - 1][0] && MapData::walkability[x][0] && MapData::walkability[x + 1][0] && !MapData::walkability[x + 2][0]) {
				MapData::walkability[x][0] = false;
				MapData::walkability[x+1][0] = false;
			}
		}
		for (size_t y = 0; y < maxHeight1; ++y) {
			if (!MapData::walkability[0][y]) {
				MapData::walkability[3][y] = false;
				MapData::walkability[2][y] = false;
				MapData::walkability[1][y] = false;
			} else {
				MapData::walkability[2][y] &= MapData::walkability[3][y];
				MapData::walkability[1][y] &= MapData::walkability[2][y];
				MapData::walkability[0][y] &= MapData::walkability[1][y];
			}
		}
		for (size_t y = 1; y < maxHeight1; ++y) {
			if (!MapData::walkability[maxWidth1][y - 1] &&
				MapData::walkability[maxWidth1][y] &&
				!MapData::walkability[maxWidth1][y + 1]) {
				MapData::walkability[maxWidth1][y] = false;
			}
		}

		// set walkability to false on static buildings
		for (const auto& unit : MapData::staticNeutralBuildings) {
			BWAPI::UnitType unitType = unit.first;
			// get build area (the position is in the middle of the unit)
			// we also expand the square by 2 walk tiles to make sure it touches any map border (like in (2)Heartbreak_Ridge.scx)
			int x1 = (unit.second.x / 8) - (unitType.tileWidth() * 2);
			int y1 = (unit.second.y / 8) - (unitType.tileHeight() * 2);
			int x2 = x1 + (unitType.tileWidth() * 4);
			int y2 = y1 + (unitType.tileHeight() * 4);
			// sanitize
			if (x1 < 0) x1 = 0;
			if (y1 < 0) y1 = 0;
			if (x2 >= MapData::mapWidthWalkRes) x2 = MapData::mapWidthWalkRes - 1;
			if (y2 >= MapData::mapHeightWalkRes) y2 = MapData::mapHeightWalkRes - 1;
			// map area
			for (int x = x1; x <= x2; x++) {
				for (int y = y1; y <= y2; y++) {
					for (int x3 = std::max(x - 1, 0); x3 <= std::min(MapData::mapWidthWalkRes - 1, x + 1); x3++) {
						for (int y3 = std::max(y - 1, 0); y3 <= std::min(MapData::mapHeightWalkRes - 1, y + 1); y3++) {
							MapData::walkability[x3][y3] = false;
						}
					}
					MapData::distanceTransform[x][y] = 0;
					MapData::lowResWalkability[x / 4][y / 4] = false;
				}
			}
		}

#ifdef OFFLINE
//		BWTA::MapData::walkability.saveToFile(std::string(BWTA_PATH)+"walkability.txt");
//		BWTA::MapData::lowResWalkability.saveToFile(std::string(BWTA_PATH)+"lowResWalkability.txt");
#endif

		BWTA_Result::getRegion.resize(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		BWTA_Result::getChokepoint.resize(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		BWTA_Result::getBaseLocation.resize(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		BWTA_Result::getChokepointW.resize(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		BWTA_Result::getBaseLocationW.resize(MapData::mapWidthWalkRes, MapData::mapHeightWalkRes);
		BWTA_Result::getUnwalkablePolygon.resize(MapData::mapWidthTileRes, MapData::mapHeightTileRes);
		BWTA_Result::getRegion.setTo(nullptr);
		BWTA_Result::getChokepoint.setTo(nullptr);
		BWTA_Result::getBaseLocation.setTo(nullptr);
		BWTA_Result::getChokepointW.setTo(nullptr);
		BWTA_Result::getBaseLocationW.setTo(nullptr);
		BWTA_Result::getUnwalkablePolygon.setTo(nullptr);
	}


  void load_data(std::string filename)
  {
    int version;
    int unwalkablePolygon_amount;
    int baselocation_amount;
    int chokepoint_amount;
    int region_amount;
    int map_width;
    int map_height;
    std::vector<PolygonImpl*> unwalkablePolygons;
    std::vector<BaseLocation*> baselocations;
    std::vector<Chokepoint*> chokepoints;
    std::vector<Region*> regions;
    std::ifstream file_in;
    file_in.open(filename.c_str());
    file_in >> version;
    if (version!=BWTA_FILE_VERSION)  {
      file_in.close();
      return;
    }
    file_in >> unwalkablePolygon_amount;
    file_in >> baselocation_amount;
    file_in >> chokepoint_amount;
    file_in >> region_amount;
    file_in >> map_width;
    file_in >> map_height;
    for(int i=0;i<unwalkablePolygon_amount;i++)
    {
      PolygonImpl* p = new PolygonImpl();
      unwalkablePolygons.push_back(p); // TODO why we need 2 vectors??
	  BWTA_Result::unwalkablePolygons.push_back(p);
    }
    for(int i=0;i<baselocation_amount;i++)
    {
      BaseLocation* b=new BaseLocationImpl();
      baselocations.push_back(b);
      BWTA_Result::baselocations.insert(b);
    }
    for(int i=0;i<chokepoint_amount;i++)
    {
      Chokepoint* c=new ChokepointImpl();
      chokepoints.push_back(c);
      BWTA_Result::chokepoints.insert(c);
    }
    for(int i=0;i<region_amount;i++)
    {
      Region* r=new RegionImpl();
      regions.push_back(r);
	  BWTA_Result::regions.push_back(r);
    }
    for(int i=0;i<unwalkablePolygon_amount;i++)
    {
      int id, polygon_size;
      file_in >> id >> polygon_size;
      for(int j=0;j<polygon_size;j++)
      {
        int x,y;
        file_in >> x >> y;
        unwalkablePolygons[i]->push_back(BWAPI::Position(x,y));
      }
      int hole_count;
      file_in >> hole_count;
      for(int j=0;j<hole_count;j++)
      {
        file_in >> polygon_size;
        PolygonImpl h;
        for(int k=0;k<polygon_size;k++)
        {
          int x,y;
          file_in >> x >> y;
          h.push_back(BWAPI::Position(x,y));
        }
		unwalkablePolygons[i]->addHole(h);
      }
    }
    for(int i=0;i<baselocation_amount;i++)
    {
      int id,px,py,tpx,tpy;
      file_in >> id >> px >> py >> tpx >> tpy;
      ((BaseLocationImpl*)baselocations[i])->position=BWAPI::Position(px,py);
      ((BaseLocationImpl*)baselocations[i])->tilePosition=BWAPI::TilePosition(tpx,tpy);
	  //LOG("Found Base Location at " << baselocations[i]->getTilePosition());
      int rid;
      file_in >> rid;
      ((BaseLocationImpl*)baselocations[i])->region=regions[rid];
      for(int j=0;j<baselocation_amount;j++)
      {
        double g_dist, a_dist;
        file_in >> g_dist >> a_dist;
        ((BaseLocationImpl*)baselocations[i])->groundDistances[baselocations[j]]=g_dist;
        ((BaseLocationImpl*)baselocations[i])->airDistances[baselocations[j]]=a_dist;
      }
      file_in >> ((BaseLocationImpl*)baselocations[i])->_isIsland;
      file_in >> ((BaseLocationImpl*)baselocations[i])->_isStartLocation;
      if (((BaseLocationImpl*)baselocations[i])->_isStartLocation)
        BWTA::BWTA_Result::startlocations.insert(baselocations[i]);
    }
    for(int i=0;i<chokepoint_amount;i++)
    {
      int id,rid1,rid2,s1x,s1y,s2x,s2y,cx,cy;
      double width;
      file_in >> id >> rid1 >> rid2 >> s1x >> s1y >> s2x >> s2y >> cx >> cy >> width;
      ((ChokepointImpl*)chokepoints[i])->_regions.first=regions[rid1];
      ((ChokepointImpl*)chokepoints[i])->_regions.second=regions[rid2];
      ((ChokepointImpl*)chokepoints[i])->_sides.first=BWAPI::Position(s1x,s1y);
      ((ChokepointImpl*)chokepoints[i])->_sides.second=BWAPI::Position(s2x,s2y);
      ((ChokepointImpl*)chokepoints[i])->_center=BWAPI::Position(cx,cy);
      ((ChokepointImpl*)chokepoints[i])->_width=width;
    }
    for(int i=0;i<region_amount;i++)
    {
      int id, polygon_size;
      file_in >> id >> polygon_size;
      for(int j=0;j<polygon_size;j++)
      {
        int x,y;
        file_in >> x >> y;
        ((RegionImpl*)regions[i])->_polygon.push_back(BWAPI::Position(x,y));
      }
      int cx,cy,chokepoints_size;
      file_in >> cx >> cy >> chokepoints_size;
      ((RegionImpl*)regions[i])->_center=BWAPI::Position(cx,cy);
      for(int j=0;j<chokepoints_size;j++)
      {
        int cid;
        file_in >> cid;
        ((RegionImpl*)regions[i])->_chokepoints.insert(chokepoints[cid]);
      }
      int baselocations_size;
      file_in >> baselocations_size;
      for(int j=0;j<baselocations_size;j++)
      {
        int bid;
        file_in >> bid;
        ((RegionImpl*)regions[i])->baseLocations.insert(baselocations[bid]);
      }
      for(int j=0;j<region_amount;j++)
      {
        int connected=0;
        file_in >> connected;
        if (connected==1)
          ((RegionImpl*)regions[i])->reachableRegions.insert(regions[j]);
      }
    }
    BWTA_Result::getRegion.resize(map_width,map_height);
    BWTA_Result::getChokepoint.resize(map_width,map_height);
    BWTA_Result::getBaseLocation.resize(map_width,map_height);
    BWTA_Result::getUnwalkablePolygon.resize(map_width,map_height);
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int rid;
        file_in >> rid;
        if (rid==-1)
          BWTA_Result::getRegion[x][y]=NULL;
        else
          BWTA_Result::getRegion[x][y]=regions[rid];
      }
    }
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int cid;
        file_in >> cid;
        if (cid==-1)
          BWTA_Result::getChokepoint[x][y]=NULL;
        else
          BWTA_Result::getChokepoint[x][y]=chokepoints[cid];
      }
    }
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int bid;
        file_in >> bid;
        if (bid==-1)
          BWTA_Result::getBaseLocation[x][y]=NULL;
        else
          BWTA_Result::getBaseLocation[x][y]=baselocations[bid];
      }
    }
    for(int x=0;x<map_width*4;x++)
    {
      for(int y=0;y<map_height*4;y++)
      {
        int cid;
        file_in >> cid;
        if (cid==-1)
          BWTA_Result::getChokepointW[x][y]=NULL;
        else
          BWTA_Result::getChokepointW[x][y]=chokepoints[cid];
      }
    }
    for(int x=0;x<map_width*4;x++)
    {
      for(int y=0;y<map_height*4;y++)
      {
        int bid;
        file_in >> bid;
        if (bid==-1)
          BWTA_Result::getBaseLocationW[x][y]=NULL;
        else
          BWTA_Result::getBaseLocationW[x][y]=baselocations[bid];
      }
    }
    for(int x=0;x<map_width;x++)
    {
      for(int y=0;y<map_height;y++)
      {
        int pid;
        file_in >> pid;
        if (pid==-1)
          BWTA_Result::getUnwalkablePolygon[x][y]=NULL;
        else
          BWTA_Result::getUnwalkablePolygon[x][y]=unwalkablePolygons[pid];
      }
    }
    file_in.close();
  }

  void save_data(std::string filename)
  {
    std::map<Polygon*,int> pid;
    std::map<BaseLocation*,int> bid;
    std::map<Chokepoint*,int> cid;
    std::map<Region*,int> rid;
    int p_id=0;
    int b_id=0;
    int c_id=0;
    int r_id=0;
    for(auto p=BWTA_Result::unwalkablePolygons.begin();p!=BWTA_Result::unwalkablePolygons.end();p++)
    {
      pid[*p]=p_id++;
    }
    for(std::set<BaseLocation*>::const_iterator b=BWTA_Result::baselocations.begin();b!=BWTA_Result::baselocations.end();b++)
    {
      bid[*b]=b_id++;
    }
    for(std::set<Chokepoint*>::const_iterator c=BWTA_Result::chokepoints.begin();c!=BWTA_Result::chokepoints.end();c++)
    {
      cid[*c]=c_id++;
    }
	for (auto& r : BWTA_Result::regions) rid[r] = r_id++;
    std::ofstream file_out;
    file_out.open(filename.c_str());
    int file_version=BWTA_FILE_VERSION;
    file_out << file_version << "\n";
    file_out << BWTA_Result::unwalkablePolygons.size() << "\n";
    file_out << BWTA_Result::baselocations.size() << "\n";
    file_out << BWTA_Result::chokepoints.size() << "\n";
    file_out << BWTA_Result::regions.size() << "\n";
    file_out << BWTA_Result::getRegion.getWidth() << "\n";
    file_out << BWTA_Result::getRegion.getHeight() << "\n";
    for(auto p=BWTA_Result::unwalkablePolygons.begin();p!=BWTA_Result::unwalkablePolygons.end();p++)
    {
      file_out << pid[*p] << "\n";
      file_out << (*p)->size() << "\n";
      for(unsigned int i=0;i<(*p)->size();i++)
      {
        file_out << (**p)[i].x << "\n";
        file_out << (**p)[i].y << "\n";
      }
      file_out << (*p)->getHoles().size() << "\n";
	  for (const auto& h : (*p)->getHoles()) {
        file_out << h->size() << "\n";
        for(unsigned int i=0;i<h->size();i++)
        {
          file_out << h->at(i).x << "\n";
          file_out << h->at(i).y << "\n";
        }
      }
    }
    for(std::set<BaseLocation*>::const_iterator b=BWTA_Result::baselocations.begin();b!=BWTA_Result::baselocations.end();b++)
    {
      file_out << bid[*b] << "\n";
      file_out << (*b)->getPosition().x << "\n";
      file_out << (*b)->getPosition().y << "\n";
      file_out << (*b)->getTilePosition().x << "\n";
      file_out << (*b)->getTilePosition().y << "\n";
      file_out << rid[(*b)->getRegion()] << "\n";
      for(std::set<BaseLocation*>::const_iterator b2=BWTA_Result::baselocations.begin();b2!=BWTA_Result::baselocations.end();b2++)
      {
        file_out << (*b)->getGroundDistance(*b2) << "\n";
        file_out << (*b)->getAirDistance(*b2) << "\n";
      }
      file_out << (*b)->isIsland() << "\n";
      file_out << (*b)->isStartLocation() << "\n";
    }
    for(std::set<Chokepoint*>::const_iterator c=BWTA_Result::chokepoints.begin();c!=BWTA_Result::chokepoints.end();c++)
    {
      file_out << cid[*c] << "\n";
      file_out << rid[(*c)->getRegions().first] << "\n";
      file_out << rid[(*c)->getRegions().second] << "\n";
      file_out << (*c)->getSides().first.x << "\n";
      file_out << (*c)->getSides().first.y << "\n";
      file_out << (*c)->getSides().second.x << "\n";
      file_out << (*c)->getSides().second.y << "\n";
      file_out << (*c)->getCenter().x << "\n";
      file_out << (*c)->getCenter().y << "\n";
      file_out << (*c)->getWidth() << "\n";
    }
	for (const auto& r : BWTA_Result::regions) {
      file_out << rid[r] << "\n";
      PolygonImpl poly = r->getPolygon();
      file_out << poly.size() << "\n";
      for(unsigned int i=0;i<poly.size();i++)
      {
        file_out << poly[i].x << "\n";
        file_out << poly[i].y << "\n";
      }
      file_out << r->getCenter().x << "\n";
      file_out << r->getCenter().y << "\n";
      file_out << r->getChokepoints().size() << "\n";
      for(std::set<Chokepoint*>::const_iterator c=r->getChokepoints().begin();c!=r->getChokepoints().end();c++)
      {
        file_out << cid[*c] << "\n";
      }
      file_out << r->getBaseLocations().size() << "\n";
      for(std::set<BaseLocation*>::const_iterator b=r->getBaseLocations().begin();b!=r->getBaseLocations().end();b++)
      {
        file_out << bid[*b] << "\n";
      }
	  for (const auto& r2 : BWTA_Result::regions) {
		  int connected = 0;
		  if (r->isReachable(r2)) connected = 1;
		  file_out << connected << "\n";
      }
    }
    for(int x=0;x<(int)BWTA_Result::getRegion.getWidth();x++)
    {
      for(int y=0;y<(int)BWTA_Result::getRegion.getHeight();y++)
      {
        if (BWTA_Result::getRegion[x][y]==NULL)
          file_out << "-1\n";
        else
          file_out << rid[BWTA_Result::getRegion[x][y]] << "\n";
      }
    }
    for(int x=0;x<(int)BWTA_Result::getChokepoint.getWidth();x++)
    {
      for(int y=0;y<(int)BWTA_Result::getChokepoint.getHeight();y++)
      {
        if (BWTA_Result::getChokepoint[x][y]==NULL)
          file_out << "-1\n";
        else
          file_out << cid[BWTA_Result::getChokepoint[x][y]] << "\n";
      }
    }
    for(int x=0;x<(int)BWTA_Result::getBaseLocation.getWidth();x++)
    {
      for(int y=0;y<(int)BWTA_Result::getBaseLocation.getHeight();y++)
      {
        if (BWTA_Result::getBaseLocation[x][y]==NULL)
          file_out << "-1\n";
        else
          file_out << bid[BWTA_Result::getBaseLocation[x][y]] << "\n";
      }
    }
    for(int x=0;x<(int)BWTA_Result::getChokepointW.getWidth();x++)
    {
      for(int y=0;y<(int)BWTA_Result::getChokepointW.getHeight();y++)
      {
        if (BWTA_Result::getChokepointW[x][y]==NULL)
          file_out << "-1\n";
        else
          file_out << cid[BWTA_Result::getChokepointW[x][y]] << "\n";
      }
    }
    for(int x=0;x<(int)BWTA_Result::getBaseLocationW.getWidth();x++)
    {
      for(int y=0;y<(int)BWTA_Result::getBaseLocationW.getHeight();y++)
      {
        if (BWTA_Result::getBaseLocationW[x][y]==NULL)
          file_out << "-1\n";
        else
          file_out << bid[BWTA_Result::getBaseLocationW[x][y]] << "\n";
      }
    }
    for(int x=0;x<(int)BWTA_Result::getRegion.getWidth();x++)
    {
      for(int y=0;y<(int)BWTA_Result::getRegion.getHeight();y++)
      {
        if (BWTA_Result::getUnwalkablePolygon[x][y]==NULL)
          file_out << "-1\n";
        else
          file_out << pid[BWTA_Result::getUnwalkablePolygon[x][y]] << "\n";
      }
    }
    file_out.close();
  }
}