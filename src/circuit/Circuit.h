/*
 * Circuit.h
 *
 *  Created on: Aug 9, 2014
 *      Author: rlcevg
 */

#ifndef CIRCUIT_H_
#define CIRCUIT_H_

#include "OOAICallback.h"
#include "SSkirmishAICallback.h"

namespace circuit {

class CCircuit {
public:
	bool initialized;

	CCircuit(springai::OOAICallback* callback);
	virtual ~CCircuit();

	void Init(int skirmishAIId, const SSkirmishAICallback* skirmishCallback);
	void Release(int reason);
	void Update(int frame);

private:
	springai::OOAICallback* callback;
	springai::Log* log;
	springai::Game* game;
	springai::Map* map;
};

} // namespace circuit

#endif // CIRCUIT_H_
