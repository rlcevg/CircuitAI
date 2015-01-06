/*
 * BlockMask.cpp
 *
 *  Created on: Dec 11, 2014
 *      Author: rlcevg
 */

#include "terrain/BlockMask.h"
#include "util/utils.h"

#include <algorithm>

namespace circuit {

IBlockMask::IBlockMask(SBlockingMap::StructType structType, int ignoreMask) :
		xsize(0),
		zsize(0),
		offsetSouth(0, 0),
		offsetEast(0, 0),
		offsetNorth(0, 0),
		offsetWest(0, 0),
		structType(structType),
		ignoreMask(ignoreMask)
{
}

IBlockMask::~IBlockMask()
{
}

IBlockMask::BlockRects IBlockMask::Init(const int2& offset, const int2& bsize, const int2& ssize)
{
	/*
	 * b - blocker, s - struct
	 *
	 * b1.x    s1.x  s2.x    b2.x          offset.x      ---x
	 * |       |     |       |                |          |
	 * mmmmmmmmmmmmmmmmmmmmmm - b1.y    mmmmmmmmmmmmm    y(z)
	 * mmmmmmmmmmmmmmmmmmmmmm           mmmmmmmmmmmmm
	 * mmmmmmmmxxxxxxmmmmmmmm - s1.y    mmmxxxoxmmmmm - offset.y
	 * mmmmmmmmxxxxxxmmmmmmmm           mmmxxxoxmmmmm
	 * mmmmmmmmxxxxxxmmmmmmmm           mmmxxooxmmmmm
	 * mmmmmmmmmmmmmmmmmmmmmm - s2.y       xxxxx
	 * mmmmmmmmmmmmmmmmmmmmmm              xxxxx
	 *                        - b2.y
	 */
	int2 bcenter(0, 0);
	int2 mcenter = bcenter + offset;

	int2 b1 = mcenter - bsize / 2;
	int2 b2 = b1 + bsize;
	int2 s1 = bcenter - ssize / 2;
	int2 s2 = s1 + ssize;

	// make positive
	int2 addon(std::min(b1.x, s1.x), std::min(b1.y, s1.y));
	b1 -= addon;
	b2 -= addon;
	s1 -= addon;
	s2 -= addon;

	xsize = std::max(b2.x, s2.x) - std::min(b1.x, s1.x);
	zsize = std::max(b2.y, s2.y) - std::min(b1.y, s1.y);
	offsetSouth.x = (b1.x < s1.x) ? (s1.x - b1.x) : 0;
	offsetSouth.y = (b1.y < s1.y) ? (s1.y - b1.y) : 0;
	offsetEast.x  = offsetSouth.y;
	offsetEast.y  = (b2.x > s2.x) ? (b2.x - s2.x) : 0;
	offsetNorth.x = offsetSouth.x;
	offsetNorth.y = (b2.y > s2.y) ? (b2.y - s2.y) : 0;
	offsetWest.x  = offsetNorth.y;
	offsetWest.y  = offsetSouth.x;

	mask.resize(xsize * zsize, BlockType::OPEN);

	return {b1, b2, s1, s2};
}

int IBlockMask::GetXSize()
{
	return xsize;
}

int IBlockMask::GetZSize()
{
	return zsize;
}

const int2& IBlockMask::GetStructOffset(int facing)
{
	switch (facing) {
		default:
		case UNIT_FACING_SOUTH: {
			return offsetSouth;
		}
		case UNIT_FACING_NORTH: {
			return offsetNorth;
		}
		case UNIT_FACING_EAST: {
			return offsetEast;
		}
		case UNIT_FACING_WEST: {
			return offsetWest;
		}
	}
}

} // namespace circuit
