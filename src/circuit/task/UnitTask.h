/*
 * UnitTask.h
 *
 *  Created on: Sep 2, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_TASK_UNITTASK_H_
#define SRC_CIRCUIT_TASK_UNITTASK_H_

#include "script/RefCounter.h"

#include "AIFloat3.h"

#include <set>
#include <map>
#include <memory>

namespace springai {
	class Unit;
}

namespace circuit {

class CCircuitUnit;
class CEnemyInfo;
class ITaskManager;
class IPathQuery;
class CQueryPathSingle;
class CQueryPathMulti;

class IUnitTask: public IRefCounter {  // CSquad, IAction
public:
	enum class Priority: char {LOW = 0, NORMAL = 1, HIGH = 2, NOW = 99};
	enum class Type: char {NIL, PLAYER, IDLE, WAIT, RETREAT, BUILDER, FACTORY, FIGHTER};
	enum class State: char {ROAM, ENGAGE, DISENGAGE, REGROUP};

protected:
	IUnitTask(ITaskManager* mgr, Priority priority, Type type, int timeout);
	virtual ~IUnitTask();
public:
	virtual void ClearRelease();

	virtual bool CanAssignTo(CCircuitUnit* unit) const;
	virtual void AssignTo(CCircuitUnit* unit);
	virtual void RemoveAssignee(CCircuitUnit* unit);

	virtual void Start(CCircuitUnit* unit) = 0;  // <=> IAction::OnStart()
	virtual void Update() = 0;
	virtual void Stop(bool done);  // <=> IAction::OnEnd()
protected:
	// NOTE: Do not run time consuming code here. Instead create separate task.
	virtual void Finish();
	virtual void Cancel();  // TODO: Make pure virtual?

public:
	virtual void OnUnitIdle(CCircuitUnit* unit) = 0;
	virtual void OnUnitDamaged(CCircuitUnit* unit, CEnemyInfo* attacker) = 0;
	virtual void OnUnitDestroyed(CCircuitUnit* unit, CEnemyInfo* attacker) = 0;
	void OnUnitMoveFailed(CCircuitUnit* unit);

	virtual void OnTravelEnd(CCircuitUnit* unit);
	virtual void OnRearmStart(CCircuitUnit* unit);

	const std::set<CCircuitUnit*>& GetAssignees() const { return units; }
	Priority GetPriority() const { return priority; }
	Type GetType() const { return type; }
	ITaskManager* GetManager() const { return manager; }

	int GetLastTouched() const { return lastTouched; }
	int GetTimeout() const { return timeout; }

	void Dead() { isDead = true; }
	bool IsDead() const { return isDead; }

protected:
	bool IsQueryReady(CCircuitUnit* unit) const;
	bool IsQueryAlive(const IPathQuery* query) const;

public:
	friend std::ostream& operator<<(std::ostream& os, const IUnitTask& data);
	friend std::istream& operator>>(std::istream& is, IUnitTask& data);
protected:
	virtual void Load(std::istream& is);
	virtual void Save(std::ostream& os) const;

	ITaskManager* manager;
	std::set<CCircuitUnit*> units;
	Priority priority;
	Type type;
	State state;
	std::map<CCircuitUnit*, std::shared_ptr<IPathQuery>> pathQueries;  // IPathQuery owner

	int lastTouched;
	int timeout;

	unsigned int updCount;
	bool isDead;

#ifdef DEBUG_VIS
public:
	virtual void Log();
#endif
};

inline std::ostream& operator<<(std::ostream& os, const IUnitTask& data)
{
	data.Save(os);
	return os;
}

inline std::istream& operator>>(std::istream& is, IUnitTask& data)
{
	data.Load(is);
	return is;
}

} // namespace circuit

#endif // SRC_CIRCUIT_TASK_UNITTASK_H_
