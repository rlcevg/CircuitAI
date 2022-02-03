#ifndef __STDAFX_H__843277f30c045b71f5603638594ff609____
#define __STDAFX_H__843277f30c045b71f5603638594ff609____

#pragma message("Compiling precompiled headers (you should see this only once)")

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define NOMINMAX

// standard includes
#include <vector>
#include <map>
#include <list>
#include <set>
#include <iostream>
#include <fstream>
#include <cassert>
#include <string>
#include <sstream>

// BWAPI
#include <BWAPI.h>

// BOOST
#pragma warning(disable: 4244) // known precision conversion warning with algorithms::buffer http://lists.boost.org/boost-users/2015/05/84281.php
#include <boost/geometry.hpp>
#pragma warning(default: 4244) 
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/polygon/voronoi.hpp>
#include <boost/polygon/polygon.hpp>
namespace bgi = boost::geometry::index;

using BoostPoint = boost::geometry::model::d2::point_xy<double>;
using BoostPolygon = boost::geometry::model::polygon<BoostPoint>;

//#define PI 3.1415926

// Internal utilities
#include "Timer.h"
#include <BWTA/RectangleArray.h>

#ifdef OFFLINE
#define BWTA_PATH "logs/"
const std::string LOG_FILE_PATH = "logs/BWTA.log";
#define LOG(message) { std::cout << message << std::endl; }
#else
#define BWTA_PATH "bwapi-data/BWTA2/"
const std::string LOG_FILE_PATH = "bwapi-data/logs/BWTA.log";
#define LOG(message) { \
	  std::ofstream logFile(LOG_FILE_PATH , std::ios_base::out | std::ios_base::app ); \
	  logFile << message << std::endl; }
#endif

#endif
