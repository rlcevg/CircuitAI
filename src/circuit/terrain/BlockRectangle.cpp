/*
 * BlockRectangle.cpp
 *
 *  Created on: Dec 11, 2014
 *      Author: rlcevg
 */

#include "terrain/BlockRectangle.h"
#include "util/utils.h"

namespace circuit {

CBlockRectangle::CBlockRectangle(const int2& offset, const int2& bsize, const int2& ssize, SBlockingMap::StructType structType, int ignoreMask) :
		IBlockMask(structType, ignoreMask)
{
	BlockRects rects = Init(offset, bsize, ssize);
	int2& b1 = rects.b1;
	int2& b2 = rects.b2;
	int2& s1 = rects.s1;
	int2& s2 = rects.s2;

	for (int z = 0; z < zsize; z++) {
		for (int x = 0; x < xsize; x++) {
			if ((x >= s1.x) && (x < s2.x) && (z >= s1.y) && (z < s2.y)) {
				mask[z * xsize + x] = BlockType::STRUCT;
			} else if ((x >= b1.x) && (x < b2.x) && (z >= b1.y) && (z < b2.y)) {
				mask[z * xsize + x] = BlockType::BLOCKED;
			} else {
				mask[z * xsize + x] = BlockType::OPEN;
			}
		}
	}
}

CBlockRectangle::~CBlockRectangle()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

} // namespace circuit
