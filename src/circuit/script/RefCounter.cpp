/*
 * RefCounter.cpp
 *
 *  Created on: Apr 6, 2019
 *      Author: rlcevg
 */

#include "script/RefCounter.h"

namespace circuit {

IRefCounter::IRefCounter()
		: refCount(1)
{
}

IRefCounter::~IRefCounter()
{
}

int IRefCounter::AddRef()
{
	return ++refCount;  // atomic
}

int IRefCounter::Release()
{
	if (--refCount == 0) {  // atomic
		delete this;
		return 0;
	}
	return refCount;
}

int IRefCounter::GetRefCount() const
{
	return refCount;
}

} // namespace circuit
