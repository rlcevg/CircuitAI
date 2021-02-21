/*
 * SchedulerJob.h
 *
 *  Created on: Feb 17, 2021
 *      Author: rlcevg
 *         See: gcc/dist/libstdc++-v3/src/c++11/thread.cc
 */

#ifndef SRC_CIRCUIT_SCHEDULER_SCHEDULERJOB_H_
#define SRC_CIRCUIT_SCHEDULER_SCHEDULERJOB_H_

#include <memory>
#include <functional>

namespace circuit {

class IPathQuery;

/*
 * IMainJob
 */
class IMainJob {
protected:
	IMainJob() {}
public:
	inline virtual ~IMainJob() = default;

	virtual void Run() = 0;
};

template<typename _Callable>
class CGameJob : public IMainJob {
public:
	CGameJob(_Callable&& __f) : _M_func(std::forward<_Callable>(__f)) {}

	virtual void Run() override { _M_func(); }

private:
	_Callable _M_func;
};

template<typename _Callable>
std::shared_ptr<CGameJob<_Callable>> _M_make_game_routine(_Callable&& __f) {
	return std::make_shared<CGameJob<_Callable>>(std::forward<_Callable>(__f));
}

template<typename _Callable>
class CPathedJob : public IMainJob {
public:
	CPathedJob(const std::shared_ptr<IPathQuery>& query, _Callable&& __f)
		: _M_func(std::forward<_Callable>(__f)), query(query) {}

	virtual void Run() override {
		std::shared_ptr<IPathQuery> pQuery = query.lock();
		if (pQuery != nullptr) {
			_M_func(pQuery.get());
		}
	}

private:
	_Callable _M_func;

	std::weak_ptr<IPathQuery> query;
};

template<typename _Callable>
std::shared_ptr<CPathedJob<_Callable>> _M_make_pathed_routine(const std::shared_ptr<IPathQuery>& query, _Callable&& __f) {
	return std::make_shared<CPathedJob<_Callable>>(query, std::forward<_Callable>(__f));
}

/*
 * IThreadJob
 */
class IThreadJob {
protected:
	IThreadJob() {}
public:
	inline virtual ~IThreadJob() = default;

	virtual std::shared_ptr<IMainJob> Run(int num) = 0;
};

template<typename _Callable>
class CWorkJob : public IThreadJob {
public:
	CWorkJob(_Callable&& __f) : _M_func(std::forward<_Callable>(__f)) {}

	virtual std::shared_ptr<IMainJob> Run(int num) override { return _M_func(); }

private:
	_Callable _M_func;
};

template<typename _Callable>
std::shared_ptr<CWorkJob<_Callable>> _M_make_work_routine(_Callable&& __f) {
	return std::make_shared<CWorkJob<_Callable>>(std::forward<_Callable>(__f));
}

template<typename _Callable>
class CPathJob : public IThreadJob {
public:
	CPathJob(const std::shared_ptr<IPathQuery>& query, _Callable&& __f)
		: _M_func(std::forward<_Callable>(__f)), query(query) {}

	virtual std::shared_ptr<IMainJob> Run(int num) override {
		std::shared_ptr<IPathQuery> pQuery = query.lock();
		if (pQuery != nullptr) {
			return _M_func(num, pQuery.get());
		}
		return nullptr;
	}

private:
	_Callable _M_func;

	std::weak_ptr<IPathQuery> query;
};

template<typename _Callable>
std::shared_ptr<CPathJob<_Callable>> _M_make_path_routine(const std::shared_ptr<IPathQuery>& query, _Callable&& __f) {
	return std::make_shared<CPathJob<_Callable>>(query, std::forward<_Callable>(__f));
}

} // namespace circuit

#endif // SRC_CIRCUIT_SCHEDULER_SCHEDULERJOB_H_
