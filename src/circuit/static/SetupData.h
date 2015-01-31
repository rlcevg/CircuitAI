/*
 * SetupData.h
 *
 *  Created on: Aug 10, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_STATIC_SETUPDATA_H_
#define SRC_CIRCUIT_STATIC_SETUPDATA_H_

#include "Game/GameSetup.h"

#include <vector>

namespace springai {
	class AIFloat3;
}

namespace circuit {

class CSetupData {
public:
	union Box {
		struct {
			float bottom;
			float left;
			float right;
			float top;
		};
		float edge[4];

		bool ContainsPoint(const springai::AIFloat3& point) const;
	};

public:
	CSetupData();
	virtual ~CSetupData();
	void Init(std::vector<Box>& sb, CGameSetup::StartPosType spt = CGameSetup::StartPosType::StartPos_ChooseInGame);

	bool IsInitialized();
	bool IsEmpty();
	bool CanChooseStartPos();
//	int GetAllyTeamsCount();

	const Box& operator[](int idx) const;

private:
	bool initialized;
	std::vector<Box> startBoxes;
	CGameSetup::StartPosType startPosType;
};

} // namespace circuit

#endif // SRC_CIRCUIT_STATIC_SETUPDATA_H_
