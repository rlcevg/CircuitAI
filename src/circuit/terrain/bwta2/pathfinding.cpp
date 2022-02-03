#include "Pathfinding.h"

namespace BWTA
{
  double AstarSearchDistance(BWAPI::TilePosition start, BWAPI::TilePosition end)
  {
    Heap<BWAPI::TilePosition,int> openTiles(true);
    std::map<BWAPI::TilePosition,int> gmap;
    std::set<BWAPI::TilePosition> closedTiles;
    openTiles.push(std::make_pair(start,0));
    gmap[start]=0;
    while(!openTiles.empty())
    {
      BWAPI::TilePosition p=openTiles.top().first;
      if (p==end)
        return gmap[p]*32.0/10.0;
      int fvalue=openTiles.top().second;
      int gvalue=gmap[p];
      openTiles.pop();
      closedTiles.insert(p);
	  int minx = std::max(p.x - 1, 0);
	  int maxx = std::min(p.x + 1, MapData::mapWidthTileRes - 1);
	  int miny = std::max(p.y - 1, 0);
	  int maxy = std::min(p.y + 1, MapData::mapHeightTileRes - 1);
      for(int x=minx;x<=maxx;x++)
        for(int y=miny;y<=maxy;y++)
        {
          if (!MapData::lowResWalkability[x][y]) continue;
          if (p.x != x && p.y != y && !MapData::lowResWalkability[p.x][y] && !MapData::lowResWalkability[x][p.y]) continue;
          BWAPI::TilePosition t(x,y);
          if (closedTiles.find(t)!=closedTiles.end()) continue;

          int g=gvalue+10; if (x!=p.x && y!=p.y) g+=4;
		  int dx = std::abs(x - end.x); int dy = std::abs(y - end.y);
		  int h = std::abs(dx - dy) * 10 + std::min(dx, dy) * 14;
          int f=g+h;
          if (gmap.find(t)==gmap.end() || gmap[t]>g)
          {
            gmap[t]=g;
            openTiles.set(t,f);
          }
        }
    }
    return -1;
  }
  std::pair<BWAPI::TilePosition,double> AstarSearchDistance(BWAPI::TilePosition start, std::set<BWAPI::TilePosition>& end)
  {
    Heap<BWAPI::TilePosition,int> openTiles(true);
    std::map<BWAPI::TilePosition,int> gmap;
    std::set<BWAPI::TilePosition> closedTiles;
    openTiles.push(std::make_pair(start,0));
    gmap[start]=0;
    while(!openTiles.empty())
    {
      BWAPI::TilePosition p=openTiles.top().first;
      if (end.find(p)!=end.end())
        return std::make_pair(p,gmap[p]*32.0/10.0);
      int fvalue=openTiles.top().second;
      int gvalue=gmap[p];
      openTiles.pop();
      closedTiles.insert(p);
	  int minx = std::max(p.x - 1, 0);
	  int maxx = std::min(p.x + 1, MapData::mapWidthTileRes - 1);
	  int miny = std::max(p.y - 1, 0);
	  int maxy = std::min(p.y + 1, MapData::mapHeightTileRes - 1);
      for(int x=minx;x<=maxx;x++)
        for(int y=miny;y<=maxy;y++)
        {
          if (!MapData::lowResWalkability[x][y]) continue;
          if (p.x != x && p.y != y && !MapData::lowResWalkability[p.x][y] && !MapData::lowResWalkability[x][p.y]) continue;
          BWAPI::TilePosition t(x,y);
          if (closedTiles.find(t)!=closedTiles.end()) continue;

          int g=gvalue+10; if (x!=p.x && y!=p.y) g+=4;
          int h=-1;
          for(std::set<BWAPI::TilePosition>::iterator i=end.begin();i!=end.end();i++)
          {
			  int dx = std::abs(x - i->x); int dy = std::abs(y - i->y);
			  int ch = std::abs(dx - dy) * 10 + std::min(dx, dy) * 14;
            if (h==-1 || ch<h)
              h=ch;
          }
          int f=g+h;
          if (gmap.find(t)==gmap.end() || gmap[t]>g)
          {
            gmap[t]=g;
            openTiles.set(t,f);
          }
        }
    }
    return std::make_pair(BWAPI::TilePositions::None,-1);
  }
  std::map<BWAPI::TilePosition,double> AstarSearchDistanceAll(BWAPI::TilePosition start, std::set<BWAPI::TilePosition>& end)
  {
    Heap<BWAPI::TilePosition,int> openTiles(true);
    std::map<BWAPI::TilePosition,int> gmap;
    std::set<BWAPI::TilePosition> closedTiles;
    openTiles.push(std::make_pair(start,0));
    gmap[start]=0;
    std::map<BWAPI::TilePosition,double> result;
    while(!openTiles.empty())
    {
      BWAPI::TilePosition p=openTiles.top().first;
      if (end.find(p)!=end.end())
      {
        result[p]=gmap[p]*32.0/10.0;
        end.erase(p);
        if (end.empty())
          return result;
      }
      int fvalue=openTiles.top().second;
      int gvalue=gmap[p];
      openTiles.pop();
      closedTiles.insert(p);
	  int minx = std::max(p.x - 1, 0);
	  int maxx = std::min(p.x + 1, MapData::mapWidthTileRes - 1);
	  int miny = std::max(p.y - 1, 0);
	  int maxy = std::min(p.y + 1, MapData::mapHeightTileRes - 1);
      for(int x=minx;x<=maxx;x++)
        for(int y=miny;y<=maxy;y++)
        {
          if (!MapData::lowResWalkability[x][y]) continue;
          if (p.x != x && p.y != y && !MapData::lowResWalkability[p.x][y] && !MapData::lowResWalkability[x][p.y]) continue;
          BWAPI::TilePosition t(x,y);
          if (closedTiles.find(t)!=closedTiles.end()) continue;

          int g=gvalue+10; if (x!=p.x && y!=p.y) g+=4;
          int h=-1;
          for(std::set<BWAPI::TilePosition>::iterator i=end.begin();i!=end.end();i++)
          {
			  int dx = std::abs(x - i->x); int dy = std::abs(y - i->y);
			  int ch = std::abs(dx - dy) * 10 + std::min(dx, dy) * 14;
            if (h==-1 || ch<h)
              h=ch;
          }
          int f=g+h;
          if (gmap.find(t)==gmap.end() || gmap[t]>g)
          {
            gmap[t]=g;
            openTiles.set(t,f);
          }
        }
    }
    return result;
  }
  std::vector<BWAPI::TilePosition> AstarSearchPath(BWAPI::TilePosition start, BWAPI::TilePosition end)
  {

    Heap<BWAPI::TilePosition,int> openTiles(true);
    std::map<BWAPI::TilePosition,int> gmap;
    std::map<BWAPI::TilePosition,BWAPI::TilePosition> parent;
    std::set<BWAPI::TilePosition> closedTiles;
    openTiles.push(std::make_pair(start,0));
    gmap[start]=0;
    parent[start]=start;
    while(!openTiles.empty())
    {
      BWAPI::TilePosition p=openTiles.top().first;
      if (p==end)
      {
        std::vector<BWAPI::TilePosition> reverse_path;
        while(p!=parent[p])
        {
          reverse_path.push_back(p);
          p=parent[p];
        }
        reverse_path.push_back(start);
        std::vector<BWAPI::TilePosition> path;
        for(int i=reverse_path.size()-1;i>=0;i--)
          path.push_back(reverse_path[i]);
        return path;
      }
      int fvalue=openTiles.top().second;
      int gvalue=gmap[p];
      openTiles.pop();
      closedTiles.insert(p);
	  int minx = std::max(p.x - 1, 0);
	  int maxx = std::min(p.x + 1, MapData::mapWidthTileRes - 1);
	  int miny = std::max(p.y - 1, 0);
	  int maxy = std::min(p.y + 1, MapData::mapHeightTileRes - 1);
      for(int x=minx;x<=maxx;x++)
        for(int y=miny;y<=maxy;y++)
        {
          if (!MapData::lowResWalkability[x][y]) continue;
          if (p.x != x && p.y != y && !MapData::lowResWalkability[p.x][y] && !MapData::lowResWalkability[x][p.y]) continue;
          BWAPI::TilePosition t(x,y);
          if (closedTiles.find(t)!=closedTiles.end()) continue;

          int g=gvalue+10;
          if (x!=p.x && y!=p.y) g+=4;
		  int dx = std::abs(x - end.x);
		  int dy = std::abs(y - end.y);
		  int h = std::abs(dx - dy) * 10 + std::min(dx, dy) * 14;
          int f=g+h;
          if (gmap.find(t)==gmap.end() || g<gmap.find(t)->second)
          {
            gmap[t]=g;
            openTiles.set(t,f);
            parent[t]=p;
          }
        }
    }
    std::vector<BWAPI::TilePosition> nopath;
    return nopath;
  }
  std::vector<BWAPI::TilePosition> AstarSearchPath(BWAPI::TilePosition start, std::set<BWAPI::TilePosition> end)
  {
    Heap<BWAPI::TilePosition,int> openTiles(true);
    std::map<BWAPI::TilePosition,int> gmap;
    std::map<BWAPI::TilePosition,BWAPI::TilePosition> parent;
    std::set<BWAPI::TilePosition> closedTiles;
    openTiles.push(std::make_pair(start,0));
    gmap[start]=0;
    parent[start]=start;
    while(!openTiles.empty())
    {
      BWAPI::TilePosition p=openTiles.top().first;
      if (end.find(p)!=end.end())
      {
        std::vector<BWAPI::TilePosition> reverse_path;
        while(p!=parent[p])
        {
          reverse_path.push_back(p);
          p=parent[p];
        }
        reverse_path.push_back(start);
        std::vector<BWAPI::TilePosition> path;
        for(int i=reverse_path.size()-1;i>=0;i--)
          path.push_back(reverse_path[i]);
        return path;
      }
      int fvalue=openTiles.top().second;
      int gvalue=gmap[p];
      openTiles.pop();
      closedTiles.insert(p);
	  int minx = std::max(p.x - 1, 0);
	  int maxx = std::min(p.x + 1, MapData::mapWidthTileRes - 1);
	  int miny = std::max(p.y - 1, 0);
	  int maxy = std::min(p.y + 1, MapData::mapHeightTileRes - 1);
      for(int x=minx;x<=maxx;x++)
        for(int y=miny;y<=maxy;y++)
        {
          if (!MapData::lowResWalkability[x][y]) continue;
          if (p.x != x && p.y != y && !MapData::lowResWalkability[p.x][y] && !MapData::lowResWalkability[x][p.y]) continue;
          BWAPI::TilePosition t(x,y);
          if (closedTiles.find(t)!=closedTiles.end()) continue;

          int g=gvalue+10; if (x!=p.x && y!=p.y) g+=4;
          int h=-1;
          for(std::set<BWAPI::TilePosition>::iterator i=end.begin();i!=end.end();i++)
          {
			  int dx = std::abs(x - i->x); int dy = std::abs(y - i->y);
			  int ch = std::abs(dx - dy) * 10 + std::min(dx, dy) * 14;
            if (h==-1 || ch<h)
              h=ch;
          }
          int f=g+h;
          if (gmap.find(t)==gmap.end() || gmap[t]>g)
          {
            gmap[t]=g;
            openTiles.set(t,f);
            parent[t]=p;
          }
        }
    }
    std::vector<BWAPI::TilePosition> nopath;
    return nopath;
  }

  // ******************************
  //      HPA* implementation
  // ******************************


	void buildChokeNodes()
	{
		for (const auto& region : BWTA::getRegions()) {
			const std::set<BWTA::Chokepoint*> chokes = region->getChokepoints();
			for (std::set<BWTA::Chokepoint*>::const_iterator choke1 = chokes.begin(); choke1 != chokes.end(); ++choke1) {
				for (std::set<BWTA::Chokepoint*>::const_iterator choke2 = choke1; choke2 != chokes.end(); ++choke2) {
					if (choke1 == choke2) continue;
					// calculate cost between two chokepoints
					int cost = (int)AstarSearchDistance(BWAPI::TilePosition((*choke1)->getCenter()), BWAPI::TilePosition((*choke2)->getCenter()));
					if (cost != -1) { // save cost
						MapData::chokeNodes[*choke1].insert(std::make_pair(*choke2, cost));
						MapData::chokeNodes[*choke2].insert(std::make_pair(*choke1, cost));
					}
				}
			}
		}
	}

	ChokePath getShortestPath2(BWAPI::TilePosition start, BWAPI::TilePosition target)
	{
		// TODO check if buildChokeNodes was called (MapData::chokeNodes was populated)
		// TODO add case where start and target are in the same region
		// copy chokeNodeGraph and add final nodes
		ChokepointGraph tmpChokeNodes = MapData::chokeNodes;
		BWTA::Region* region = BWTA::getRegion(BWAPI::TilePosition(target));
		std::set<BWTA::Chokepoint*> chokes = region->getChokepoints();
		// get cost to each entrance of the target
		for (std::set<BWTA::Chokepoint*>::const_iterator it = chokes.begin(); it != chokes.end(); ++it) {
			// get cost to target
			int cost = (int)AstarSearchDistance(target, BWAPI::TilePosition((*it)->getCenter()));
			if (cost != -1) tmpChokeNodes[(*it)].insert(std::make_pair((BWTA::Chokepoint *)NULL, cost));
		}

		// Execute A* on tmpChokeNodes graph
		ChokePath path;
		std::multimap<int, BWTA::Chokepoint*> openNodes;
		std::map<BWTA::Chokepoint*, int> gmap;
		std::map<BWTA::Chokepoint*, BWTA::Chokepoint*> parent;
		std::set<BWTA::Chokepoint*> closedNodes;

		// set first nodes as the cost to get to region start entrances
		region = BWTA::getRegion(BWAPI::TilePosition(start));
		chokes = region->getChokepoints();
		// get cost to each entrance of the start
		for (std::set<BWTA::Chokepoint*>::const_iterator it = chokes.begin(); it != chokes.end(); ++it) {
			// get cost to start
			int cost = (int)AstarSearchDistance(start, BWAPI::TilePosition((*it)->getCenter()));
			if (cost != -1) {
				openNodes.insert(std::make_pair(cost, *it));
				gmap[*it] = cost;
				parent[*it] = *it;
			}
		}

		while(!openNodes.empty()) {
			BWTA::Chokepoint* p = openNodes.begin()->second;
			
			int gvalue = gmap[p];

			int fvalue = openNodes.begin()->first;

			openNodes.erase(openNodes.begin());
			closedNodes.insert(p);

			// child 
			ChokeCost childNodes = tmpChokeNodes[p];
			for (std::set< std::pair<BWTA::Chokepoint*, int> >::const_iterator child = childNodes.begin(); child != childNodes.end(); ++child) {
				BWTA::Chokepoint* t = child->first;
				if(t == NULL) { // we found the path 
					//BWTA::Chokepoint* p = openNodes.begin()->second;
					while(p != parent[p]) {
						path.push_front(p);
						p = parent[p];
					}
					path.push_front(p);
					//DEBUG(" Path size: " << path.size() );
					return path;
				}
				
				if(closedNodes.find(t) != closedNodes.end()) continue;

				int g = gvalue + child->second;
				BWAPI::TilePosition tileCenter = BWAPI::TilePosition(t->getCenter());
				int dx = std::abs(tileCenter.x - target.x);
				int dy = std::abs(tileCenter.y - target.y);
				int h = std::abs(dx - dy) * 10 + std::min(dx, dy) * 14;
				int f = g + h;
				if (gmap.find(t) == gmap.end() || g < gmap.find(t)->second) {
					gmap[t] = g;

					std::pair<std::multimap<int, BWTA::Chokepoint*>::iterator, std::multimap<int, BWTA::Chokepoint*>::iterator> itp = openNodes.equal_range(f);
					if (itp.second == itp.first) 
						openNodes.insert(std::make_pair(f, t));
					else {
						for (std::multimap<int, BWTA::Chokepoint*>::const_iterator it = itp.first; it != itp.second; ++it) {
							if (it->second == t) {
								openNodes.erase(it);
								break;
							} 
						}
						openNodes.insert(std::make_pair(f, t));
					}
					parent[t] = p;
				}
			}
		}

		return path;
	}

	int getGroundDistance2(BWAPI::TilePosition start, BWAPI::TilePosition target)
	{
		// TODO check if buildChokeNodes was called (MapData::chokeNodes was populated)
		BWTA::Region* startRegion = BWTA::getRegion(start);
		BWTA::Region* targetRegion = BWTA::getRegion(target);
		if (startRegion == nullptr || targetRegion == nullptr) {
			return -1;
		}
		if (startRegion == targetRegion) {
			return (int)AstarSearchDistance(start,target);
		}
		// copy chokeNodeGraph and add final nodes
		ChokepointGraph tmpChokeNodes = MapData::chokeNodes;
		std::set<BWTA::Chokepoint*> chokes = targetRegion->getChokepoints();
		// get cost to each entrance of the target
		for (const auto& choke : chokes) {
			// get cost to target
			int cost = (int)AstarSearchDistance(target, BWAPI::TilePosition(choke->getCenter()));
			//log("[TARGET] Cost to chokepoint (" << BWAPI::TilePosition((*it)->getCenter()).x() << "," << BWAPI::TilePosition((*it)->getCenter()).y() << ": " << cost );
			if (cost != -1) tmpChokeNodes[choke].insert(std::make_pair((BWTA::Chokepoint *)NULL, cost));
		}

		// Execute A* on tmpChokeNodes graph
		ChokePath path;
		std::multimap<int, BWTA::Chokepoint*> openNodes;
		std::map<BWTA::Chokepoint*, int> gmap;
		std::map<BWTA::Chokepoint*, BWTA::Chokepoint*> parent;
		std::set<BWTA::Chokepoint*> closedNodes;

		// set first nodes as the cost to get to region start entrances
		chokes = startRegion->getChokepoints();
		// get cost to each entrance of the start
		for (const auto& choke : chokes) {
			// get cost to start
			int cost = (int)AstarSearchDistance(start, BWAPI::TilePosition(choke->getCenter()));
			//log("[START] Cost to chokepoint (" << BWAPI::TilePosition((*it)->getCenter()).x() << "," << BWAPI::TilePosition((*it)->getCenter()).y() << ": " << cost );
			if (cost != -1) {
				openNodes.insert(std::make_pair(cost, choke));
				gmap[choke] = cost;
				parent[choke] = choke;
			}
		}

		while(!openNodes.empty()) {
			BWTA::Chokepoint* p = openNodes.begin()->second;
			
			int gvalue = gmap[p];

			int fvalue = openNodes.begin()->first;

			openNodes.erase(openNodes.begin());
			closedNodes.insert(p);

			// child 
			ChokeCost childNodes = tmpChokeNodes[p];
			for (std::set< std::pair<BWTA::Chokepoint*, int> >::const_iterator child = childNodes.begin(); child != childNodes.end(); ++child) {
				BWTA::Chokepoint* t = child->first;
				if(t == NULL) { // we found the path 
					return gvalue + child->second;
				}
				
				if(closedNodes.find(t) != closedNodes.end()) continue;

				int g = gvalue + child->second;
				BWAPI::TilePosition tileCenter = BWAPI::TilePosition(t->getCenter());
				int dx = std::abs(tileCenter.x - target.x);
				int dy = std::abs(tileCenter.y - target.y);
				int h = std::abs(dx - dy) * 10 + std::min(dx, dy) * 14;
				int f = g + h;
				if (gmap.find(t) == gmap.end() || g < gmap.find(t)->second) {
					gmap[t] = g;

					std::pair<std::multimap<int, BWTA::Chokepoint*>::iterator, std::multimap<int, BWTA::Chokepoint*>::iterator> itp = openNodes.equal_range(f);
					if (itp.second == itp.first) 
						openNodes.insert(std::make_pair(f, t));
					else {
						for (std::multimap<int, BWTA::Chokepoint*>::const_iterator it = itp.first; it != itp.second; ++it) {
							if (it->second == t) {
								openNodes.erase(it);
								break;
							} 
						}
						openNodes.insert(std::make_pair(f, t));
					}
					parent[t] = p;
				}
			}
		}

		return -1;
	}
}