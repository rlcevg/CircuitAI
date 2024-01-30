/*
 * SquadTask.cpp
 *
 *  Created on: Jan 23, 2016
 *      Author: rlcevg
 */

#include "task/fighter/SquadTask.h"
#include "map/InfluenceMap.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/MilitaryManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/path/PathFinder.h"
#include "terrain/path/QueryLineMap.h"
#include "unit/action/TravelAction.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include <cmath>

namespace circuit {

using namespace springai;
using namespace terrain;

ISquadTask::ISquadTask(IUnitModule* mgr, FightType type, float powerMod)
		: IFighterTask(mgr, type, powerMod)
		, lowestRange(std::numeric_limits<float>::max())
		, highestRange(.0f)
		, lowestSpeed(std::numeric_limits<float>::max())
		, highestSpeed(.0f)
		, leader(nullptr)
		, groupPos(-RgtVector)
		, prevGroupPos(-RgtVector)
		, pPath(std::make_shared<CPathInfo>())
		, groupFrame(0)
		, attackFrame(-1)
{
}

ISquadTask::~ISquadTask()
{
}

void ISquadTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	CCircuitDef* cdef = unit->GetCircuitDef();
	const float range = cdef->GetMinRange();
	rangeUnits[range].insert(unit);

	if (leader == nullptr) {
		lowestRange  = cdef->GetMaxRange();
		highestRange = cdef->GetMaxRange();
		lowestSpeed  = cdef->GetSpeed();
		highestSpeed = cdef->GetSpeed();
		leader = unit;
	} else {
		lowestRange  = std::min(lowestRange,  cdef->GetMaxRange());
		highestRange = std::max(highestRange, cdef->GetMaxRange());
		lowestSpeed  = std::min(lowestSpeed,  cdef->GetSpeed());
		highestSpeed = std::max(highestSpeed, cdef->GetSpeed());
		if (cdef->IsRoleSupport()) {
			return;
		}
		if ((leader->GetArea() == nullptr) ||
			leader->GetCircuitDef()->IsRoleSupport() ||
			((unit->GetArea() != nullptr) && (unit->GetArea()->percentOfMap < leader->GetArea()->percentOfMap)))
		{
			leader = unit;
		}
	}
}

void ISquadTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);

	CCircuitDef* cdef = unit->GetCircuitDef();
	const float range = cdef->GetMinRange();
	std::set<CCircuitUnit*>& setUnits = rangeUnits[range];
	setUnits.erase(unit);
	if (setUnits.empty()) {
		rangeUnits.erase(range);
	}

	leader = nullptr;
	lowestRange = lowestSpeed = std::numeric_limits<float>::max();
	highestRange = highestSpeed = .0f;

	if (units.empty()) {
		return;
	}

	FindLeader(units.begin(), units.end());
}

void ISquadTask::Merge(ISquadTask* task)
{
	const std::set<CCircuitUnit*>& rookies = task->GetAssignees();
	IAction::State state = leader->GetTravelAct()->GetState();
	const std::shared_ptr<CPathInfo>& lPath = leader->GetTravelAct()->GetPath();
	for (CCircuitUnit* unit : rookies) {
		unit->SetTask(this);
		if (unit->GetCircuitDef()->IsRoleSupport()) {
			continue;
		}
		unit->GetTravelAct()->SetPath(lPath);
		unit->GetTravelAct()->SetState(state);
	}
	units.insert(rookies.begin(), rookies.end());
	attackPower += task->GetAttackPower();
	const std::set<CCircuitUnit*>& sh = task->GetShields();
	shields.insert(sh.begin(), sh.end());

	const std::map<float, std::set<CCircuitUnit*>>& rangers = task->GetRangeUnits();
	for (const auto& kv : rangers) {
		rangeUnits[kv.first].insert(kv.second.begin(), kv.second.end());
	}

	FindLeader(rookies.begin(), rookies.end());
}

const AIFloat3& ISquadTask::GetLeaderPos(int frame) const
{
	return (leader != nullptr) ? leader->GetPos(frame) : GetPosition();
}

void ISquadTask::FindLeader(decltype(units)::iterator itBegin, decltype(units)::iterator itEnd)
{
	if (leader == nullptr) {
		for (; itBegin != itEnd; ++itBegin) {
			CCircuitUnit* ass = *itBegin;
			lowestRange  = std::min(lowestRange,  ass->GetCircuitDef()->GetMaxRange());
			highestRange = std::max(highestRange, ass->GetCircuitDef()->GetMaxRange());
			lowestSpeed  = std::min(lowestSpeed,  ass->GetCircuitDef()->GetSpeed());
			highestSpeed = std::max(highestSpeed, ass->GetCircuitDef()->GetSpeed());
			if (!ass->GetCircuitDef()->IsRoleSupport()) {
				leader = ass;
				++itBegin;
				break;
			}
		}
	}
	for (; itBegin != itEnd; ++itBegin) {
		CCircuitUnit* ass = *itBegin;
		lowestRange  = std::min(lowestRange,  ass->GetCircuitDef()->GetMaxRange());
		highestRange = std::max(highestRange, ass->GetCircuitDef()->GetMaxRange());
		lowestSpeed  = std::min(lowestSpeed,  ass->GetCircuitDef()->GetSpeed());
		highestSpeed = std::max(highestSpeed, ass->GetCircuitDef()->GetSpeed());
		if (ass->GetCircuitDef()->IsRoleSupport() || (ass->GetArea() == nullptr)) {
			continue;
		}
		if ((leader->GetArea() == nullptr) ||
			leader->GetCircuitDef()->IsRoleSupport() ||
			(ass->GetArea()->percentOfMap < leader->GetArea()->percentOfMap))
		{
			leader = ass;
		}
	}
}

bool ISquadTask::IsMergeSafe() const
{
	CCircuitAI* circuit = manager->GetCircuit();
	const AIFloat3& pos = leader->GetPos(circuit->GetLastFrame());
	return (circuit->GetInflMap()->GetInfluenceAt(pos) > -INFL_EPS);
}

ISquadTask* ISquadTask::CheckMergeTask()
{
	const ISquadTask* task = nullptr;

	CCircuitAI* circuit = manager->GetCircuit();
	const int frame = circuit->GetLastFrame();
	const AIFloat3& pos = leader->GetPos(frame);
	SArea* area = leader->GetArea();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();
	const float sqMaxDistCost = SQUARE(MAX_TRAVEL_SEC * lowestSpeed);
	float metric = std::numeric_limits<float>::max();

	CPathFinder* pathfinder = circuit->GetPathfinder();
	std::shared_ptr<CQueryLineMap> query = std::static_pointer_cast<CQueryLineMap>(
			pathfinder->CreateLineMapQuery(leader, circuit->GetThreatMap(), pos));

	const std::set<IFighterTask*>& tasks = static_cast<CMilitaryManager*>(manager)->GetTasks(fightType);
	for (const IFighterTask* candidate : tasks) {
		if ((candidate == this)
			|| (candidate->GetAttackPower() < attackPower)
			|| !candidate->CanAssignTo(leader))
		{
			continue;
		}
		const ISquadTask* candy = static_cast<const ISquadTask*>(candidate);

		const AIFloat3& tp = candy->GetLeaderPos(frame);
		const AIFloat3& taskPos = utils::is_valid(tp) ? tp : pos;

		if (!terrainMgr->CanMoveToPos(area, taskPos)) {  // ensure that path always exists
			continue;
		}

		if (!query->IsSafeLine(pos, taskPos)) {  // ensure safe passage
			continue;
		}

		// Check time-distance to target
		float sqDistCost = pos.SqDistance2D(taskPos);
		if ((sqDistCost < metric) && (sqDistCost < sqMaxDistCost)) {
			task = candy;
			metric = sqDistCost;
		}
	}

	return const_cast<ISquadTask*>(task);
}

ISquadTask* ISquadTask::GetMergeTask()
{
	if (updCount % 32 == 1) {
		return IsMergeSafe() ? CheckMergeTask() : nullptr;
	}
	return nullptr;
}

bool ISquadTask::IsMustRegroup()
{
	if ((State::ENGAGE == state) || (updCount % 16 != 15)) {
		return false;
	}

	if (!IsMergeSafe()) {  // (circuit->GetInflMap()->GetEnemyInflAt(leader->GetPos(frame)) > INFL_EPS) ?
		state = State::ROAM;
		return false;
	}

	static std::vector<CCircuitUnit*> validUnits;  // NOTE: micro-opt
//	validUnits.reserve(units.size());
	CCircuitAI* circuit = manager->GetCircuit();
	CThreatMap* threatMap = circuit->GetThreatMap();
	threatMap->SetThreatType(leader);
	const int frame = circuit->GetLastFrame();
	const AIFloat3& leadPos = leader->GetPos(frame);
	CCircuitUnit* bestPlace = leader;
	float minSqDist = std::numeric_limits<float>::max();
	CTerrainManager* terrainMgr = circuit->GetTerrainManager();;
	for (CCircuitUnit* unit : units) {
		const AIFloat3& unitPos = unit->GetPos(frame);
		if (!unit->GetCircuitDef()->IsPlane() &&
			terrainMgr->CanMoveToPos(unit->GetArea(), unitPos))
		{
			validUnits.push_back(unit);
			if ((State::REGROUP == state) || (threatMap->GetThreatAt(leadPos) >= THREAT_MIN)) {
				continue;
			}
			const float sqDist = unitPos.SqDistance2D(leadPos);
			if (minSqDist > sqDist) {
				minSqDist = sqDist;
				bestPlace = unit;
			}
		}
	}
	if (validUnits.empty()) {
		state = State::ROAM;
		return false;
	}

	if (State::REGROUP != state) {
		groupPos = bestPlace->GetLastPos();
		groupFrame = frame;
	} else if (frame >= groupFrame + FRAMES_PER_SEC * 60) {
		// eliminate buggy units
		const float sqMaxDist = SQUARE(std::max<float>(SQUARE_SIZE * 8 * validUnits.size(), highestRange));
		for (CCircuitUnit* unit : units) {
			if (unit->GetCircuitDef()->IsPlane()) {
				continue;
			}
			const AIFloat3& pos = unit->GetLastPos();
			const float sqDist = groupPos.SqDistance2D(pos);
			if ((sqDist > sqMaxDist) &&
				((unit->GetTaskFrame() < groupFrame) || !terrainMgr->CanMoveToPos(unit->GetArea(), pos)))
			{
				TRY_UNIT(circuit, unit,
					unit->CmdStop();
					unit->GetUnit()->SetMoveState(2);
				)
				circuit->Garbage(unit, "stuck");
//				circuit->GetBuilderManager()->EnqueueTask(TaskB::Reclaim(IBuilderTask::Priority::HIGH, unit));
			}
		}

		validUnits.clear();
		state = State::ROAM;
		return false;
	}

	if (threatMap->GetThreatAt(groupPos) >= THREAT_MIN) {
		validUnits.clear();
		state = State::ROAM;
		return false;
	}

	bool wasRegroup = (State::REGROUP == state);
	state = State::ROAM;

	const float sqMaxDist = SQUARE(std::max<float>(SQUARE_SIZE * 8 * validUnits.size(), highestRange));
	for (CCircuitUnit* unit : validUnits) {
		const float sqDist = groupPos.SqDistance2D(unit->GetLastPos());
		if (sqDist > sqMaxDist) {
			state = State::REGROUP;
			break;
		}
	}

	if (!wasRegroup && (State::REGROUP == state)) {
		if (utils::is_equal_pos(prevGroupPos, groupPos)) {
			TRY_UNIT(circuit, leader,
				leader->CmdStop();
				leader->GetUnit()->SetMoveState(2);
			)
			circuit->Garbage(leader, "stuck");
//			circuit->GetBuilderManager()->EnqueueTask(TaskB::Reclaim(IBuilderTask::Priority::HIGH, leader));
		}
		prevGroupPos = groupPos;
	}

	validUnits.clear();
	return State::REGROUP == state;
}

void ISquadTask::ActivePath(float speed)
{
	for (CCircuitUnit* unit : units) {
		unit->GetTravelAct()->SetPath(pPath, speed);
	}
}

NSMicroPather::HitFunc ISquadTask::GetHitTest() const
{
	CTerrainManager* terrainMgr = manager->GetCircuit()->GetTerrainManager();
	const std::vector<SSector>& sectors = terrainMgr->GetAreaData()->sector;
	const int sectorXSize = terrainMgr->GetSectorXSize();
	const int convert = terrainMgr->GetConvertStoP();
	const float aimLift = leader->GetCircuitDef()->GetHeight() * 0.5f;  // TODO: Use aim-pos of attacker and enemy
	const float maxHeight = leader->GetCircuitDef()->GetMaxRange() * 0.4f;
	return [&sectors, sectorXSize, aimLift, maxHeight, convert](int2 start, int2 end) {  // losTest
		const float startHeight = sectors[start.y * sectorXSize + start.x].maxElevation + aimLift;
		const float diffHeight = sectors[end.y * sectorXSize + end.x].maxElevation + SQUARE_SIZE - startHeight;
		// check vertical angle
		const float absDiffHeight = std::fabs(diffHeight);
		if (absDiffHeight > maxHeight) {
			const float dirX = (end.x - start.x) * convert;
			const float dirY = (end.y - start.y) * convert;
			const float len = std::sqrt(SQUARE(dirX) + SQUARE(dirY) + SQUARE(absDiffHeight));
			if (absDiffHeight > SQRT_3_2 * len) {  // cos(a) > sqrt(3)/2; a < 30 deg
				return false;
			}
		}
		// All octant line draw
		const int dx =  abs(end.x - start.x), sx = start.x < end.x ? 1 : -1;
		const int dy = -abs(end.y - start.y), sy = start.y < end.y ? 1 : -1;
		int err = dx + dy;  // error value e_xy
		for (int x = start.x, y = start.y;;) {
			const int e2 = 2 * err;
			if (e2 >= dy) {  // e_xy + e_x > 0
				if (x == end.x) break;
				err += dy; x += sx;
			}
			if (e2 <= dx) {  // e_xy + e_y < 0
				if (y == end.y) break;
				err += dx; y += sy;
			}

			const float t = std::fabs((dx > -dy) ? float(x - start.x) / dx : float(y - start.y) / dy);
			if (sectors[y * sectorXSize + x].maxElevation > diffHeight * t + startHeight) {
				return false;
			}
		}
		return true;
	};
}

void ISquadTask::Attack(const int frame)
{
	Attack(frame, GetTarget()->GetUnit()->IsCloaked());
}

void ISquadTask::Attack(const int frame, const bool isGround)
{
	const AIFloat3& tPos = GetTarget()->GetPos();
	const bool isRepeatAttack = (frame >= attackFrame + FRAMES_PER_SEC * 3);
	attackFrame = isRepeatAttack ? frame : attackFrame;

	auto it = rangeUnits.begin()->second.begin();
	std::advance(it, rangeUnits.begin()->second.size() / 2);  // TODO: Optimize
	AIFloat3 dir = (*it)->GetPos(frame) - tPos;

	if (leader->GetCircuitDef()->IsPlane() || (std::fabs(dir.y) > leader->GetCircuitDef()->GetMaxRange() * 0.5f)) {
		if (isRepeatAttack) {
			for (CCircuitUnit* unit : units) {
				if (unit->Blocker() != nullptr) {
					continue;  // Do not interrupt current action
				}
				unit->GetTravelAct()->StateWait();

				unit->Attack(GetTarget(), isGround, frame + FRAMES_PER_SEC * 60);
			}
		}
		return;
	}

	const int targetTile = manager->GetCircuit()->GetInflMap()->Pos2Index(tPos);
	const float alpha = std::atan2(dir.z, dir.x);
	CCircuitDef* edef = GetTarget()->GetCircuitDef();
	const bool isStatic = (edef != nullptr) && !edef->IsMobile();
	// incorrect, it should check aoe in vicinity
	const float aoe = (edef != nullptr) ? edef->GetAoe() : SQUARE_SIZE;

	int row = 0;
	for (const auto& kv : rangeUnits) {
		CCircuitDef* rowDef = (*kv.second.begin())->GetCircuitDef();
		const float range = kv.first * RANGE_MOD;
		// NOTE: 1st unit in 1st row will scout, ignoring GetTarget()->IsInRadarOrLOS()
		//       as unit may wobble back and forth without firing if turret turn is slow.
		float range0 = range;
		if ((row++ == 0) && (isStatic || !GetTarget()->IsInRadarOrLOS())) {
			range0 = std::min(kv.first, rowDef->GetLosRadius()) * RANGE_MOD;
		}
		const float maxDelta = (M_PI * 0.9f) / kv.second.size();
		// NOTE: float delta = asinf(cdef->GetRadius() / range);
		//       but sin of a small angle is similar to that angle, omit asinf() call
		float delta = (3.0f * (rowDef->GetRadius() + aoe)) / (range + DIV0_SLACK);
		if (delta > maxDelta) {
			delta = maxDelta;
		}

		float beta = -delta * (kv.second.size() / 2);
		const float end1 = alpha + beta;
		const float end2 = alpha - beta;
		AIFloat3 newPos1(tPos.x + range * cosf(end1), tPos.y, tPos.z + range * sinf(end1));
		AIFloat3 newPos2(tPos.x + range * cosf(end2), tPos.y, tPos.z + range * sinf(end2));
		const AIFloat3 testPos = (*kv.second.begin())->GetPos(frame);
		if (testPos.SqDistance2D(newPos1) > testPos.SqDistance2D(newPos2)) {
			delta = -delta;
			beta = -beta;
		}

		int iterNum = 0;
		for (CCircuitUnit* unit : kv.second) {
			if (unit->Blocker() != nullptr) {
				continue;  // Do not interrupt current action
			}
			unit->GetTravelAct()->StateWait();

			if (isRepeatAttack
				|| (unit->GetTarget() != GetTarget())
				|| (unit->GetTargetTile() != targetTile))
			{
				const float angle = alpha + beta;
				const float r = (iterNum == 0) ? range0 : range;
				AIFloat3 newPos(tPos.x + r * cosf(angle), tPos.y, tPos.z + r * sinf(angle));
				CTerrainManager::CorrectPosition(newPos);
				unit->Attack(newPos, GetTarget(), targetTile, isGround, isStatic, frame + FRAMES_PER_SEC * 60);
			}

			beta += delta;
			++iterNum;
		}
	}
}

#ifdef DEBUG_VIS
void ISquadTask::Log()
{
	IFighterTask::Log();

	CCircuitAI* circuit = manager->GetCircuit();
	circuit->LOG("pPath: %i | size: %i | TravelAct: %i", pPath.get(), pPath ? pPath->posPath.size() : 0,
			leader->GetTravelAct()->GetState());
	if (leader != nullptr) {
		circuit->GetDrawer()->AddPoint(leader->GetPos(circuit->GetLastFrame()), leader->GetCircuitDef()->GetDef()->GetName());
	}
}
#endif

} // namespace circuit
