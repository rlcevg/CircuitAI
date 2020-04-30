/*
 * RefCounter.h
 *
 *  Created on: Apr 6, 2019
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_SCRIPT_REFCOUNTER_H_
#define SRC_CIRCUIT_SCRIPT_REFCOUNTER_H_

namespace circuit {

/*
 * WARNING: Use asMETHODPR to register IRefCounter,
 *          asMETHOD may not detect ptrdiff_t of member function
 *          (multiple inheritance)
 */
class IRefCounter {
public:
	IRefCounter();
	virtual ~IRefCounter();

public:
	int AddRef();
	int Release();
	int GetRefCount();

private:
	int refCount;
};

class CRefHolder {
public:
	CRefHolder& operator=(const CRefHolder&) = delete;

	CRefHolder(const CRefHolder& that) : CRefHolder(that.ref) {}
	CRefHolder(IRefCounter* ref) : ref(ref) { ref->AddRef(); }
	~CRefHolder() { ref->Release(); }

private:
	IRefCounter* ref;
};

} // namespace circuit

#endif // SRC_CIRCUIT_SCRIPT_REFCOUNTER_H_
