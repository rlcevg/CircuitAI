/*
 * BlockCircle.h
 *
 *  Created on: Dec 11, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_BLOCKCIRCLE_H_
#define SRC_CIRCUIT_BLOCKCIRCLE_H_

#include "BlockMask.h"

namespace circuit {

class CBlockCircle: public virtual IBlockMask {
public:
	// ssize - struct size
	CBlockCircle(const int2& offset, int radius, const int2& ssize,
				 SBlockingMap::StructType structType = SBlockingMap::StructType::UNKNOWN, int ignoreMask = 0);
	virtual ~CBlockCircle();
};

} // namespace circuit

#endif // SRC_CIRCUIT_BLOCKCIRCLE_H_
