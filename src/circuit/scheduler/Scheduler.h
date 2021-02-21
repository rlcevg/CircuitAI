/*
 * Scheduler.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_SCHEDULER_H_
#define SRC_CIRCUIT_UTIL_SCHEDULER_H_

#include "scheduler/SchedulerJob.h"
#include "util/MultiQueue.h"
#include "util/Defines.h"

#include "System/Threading/SpringThreading.h"

#include <memory>
#include <list>

namespace circuit {

class Barrier {
public:
	template<typename _Predicate>
	void NotifyOne(_Predicate __p) {
		{
			std::lock_guard<spring::mutex> mlock(_mutex);
			__p();
		}
		_cond.notify_one();
	}
	template<typename _Predicate>
	void Wait(_Predicate __p) {
		std::unique_lock<spring::mutex> mlock(_mutex);
		_cond.wait(mlock, __p);
	}

private:
	spring::mutex _mutex;
	spring::condition_variable_any _cond;
};

class CScheduler {
public:
	CScheduler();
	virtual ~CScheduler();

	void Init(const std::shared_ptr<CScheduler>& thisPtr) { self = thisPtr; }
	void ProcessInit();
	void ProcessRelease();

private:
	void Release();
	void StartThreads();

public:
	template<typename _Callable, typename... _Args>
	static std::shared_ptr<IMainJob> GameJob(_Callable&& __f, _Args&&... __args) {
		return _M_make_game_routine(std::bind(std::forward<_Callable>(__f), std::forward<_Args>(__args)...));
	}
	template<typename _Callable, typename... _Args>
	static std::shared_ptr<IMainJob> PathedJob(const std::shared_ptr<IPathQuery>& query, _Callable&& __f, _Args&&... __args) {
		return _M_make_pathed_routine(query, std::bind(std::forward<_Callable>(__f),
				std::placeholders::_1, std::forward<_Args>(__args)...));
	}
	template<typename _Callable, typename... _Args>
	static std::shared_ptr<IThreadJob> WorkJob(_Callable&& __f, _Args&&... __args) {
		return _M_make_work_routine(std::bind(std::forward<_Callable>(__f), std::forward<_Args>(__args)...));
	}
	template<typename _Callable, typename... _Args>
	static std::shared_ptr<IThreadJob> PathJob(const std::shared_ptr<IPathQuery>& query, _Callable&& __f, _Args&&... __args) {
		return _M_make_path_routine(query, std::bind(std::forward<_Callable>(__f),
				std::placeholders::_1, std::placeholders::_2, std::forward<_Args>(__args)...));
	}

	/*
	 * Add task at specified frame, or execute immediately at next frame
	 */
	void RunTaskAt(const std::shared_ptr<IMainJob>& task, int frame = 0) {
		onceTasks.push_back({task, frame});
	}

	/*
	 * Add task at frame relative to current frame
	 */
	void RunTaskAfter(const std::shared_ptr<IMainJob>& task, int frame = 0) {
		onceTasks.push_back({task, lastFrame + frame});
	}

	/*
	 * Add task at specified interval
	 */
	void RunTaskEvery(const std::shared_ptr<IMainJob>& task, int frameInterval = FRAMES_PER_SEC, int frameOffset = 0);

	/*
	 * Process queued tasks at specified frame
	 */
	void ProcessTasks(int frame);

	/*
	 * Run concurrent task, finalize on success at main thread
	 */
	void RunParallelJob(const std::shared_ptr<IThreadJob>& task);

	/*
	 * Same as RunParallelTask but pushes task in front of the queue
	 */
	void RunPriorityJob(const std::shared_ptr<IThreadJob>& task);

	/*
	 * Remove scheduled task from queue
	 */
	void RemoveJob(const std::shared_ptr<IMainJob>& task);

	/*
	 * Run task on init. Not affected by RemoveTask
	 */
	void RunOnInit(std::shared_ptr<IMainJob>&& task) {
		initTasks.push_back(task);
	}

	/*
	 * Run task on release. Not affected by RemoveTask
	 */
	void RunOnRelease(std::shared_ptr<IMainJob>&& task) {
		releaseTasks.push_back(task);
	}

	int GetMaxPathThreads() const { return maxWorkThreads; }

private:
	std::weak_ptr<CScheduler> self;
	int lastFrame;
	bool isProcessing;  // regular IMainJob

	int numWorkProcess;  // parallel IThreadJob
	Barrier barrier;
	std::atomic<bool> isRunning;  // parallel

	struct BaseContainer {
		BaseContainer(const std::shared_ptr<IMainJob>& task) : task(task) {}
		std::shared_ptr<IMainJob> task;
		bool operator==(const BaseContainer& other) const {
			return task == other.task;
		}
	};
	struct OnceTask: public BaseContainer {
		OnceTask(const std::shared_ptr<IMainJob>& task, int frame)
			: BaseContainer(task), frame(frame) {}
		int frame;
	};
	std::list<OnceTask> onceTasks;

	struct RepeatTask: public BaseContainer {
		RepeatTask(const std::shared_ptr<IMainJob>& task, int frameInterval, int lastFrame)
			: BaseContainer(task), frameInterval(frameInterval), lastFrame(lastFrame) {}
		int frameInterval;
		int lastFrame;
	};
	std::list<RepeatTask> repeatTasks;

	std::vector<std::shared_ptr<IMainJob>> removeTasks;

	struct WorkTask {
		WorkTask(const std::weak_ptr<CScheduler>& scheduler, const std::shared_ptr<IThreadJob>& task)
			: scheduler(scheduler), task(task) {}
		std::weak_ptr<CScheduler> scheduler;
		std::shared_ptr<IThreadJob> task;
	};
	static CMultiQueue<WorkTask> workTasks;

	struct FinishTask: public BaseContainer {
		FinishTask(const std::shared_ptr<IMainJob>& task)
			: BaseContainer(task) {}
	};
	CMultiQueue<FinishTask> finishTasks;  // WorkTask.onComplete

	std::vector<std::shared_ptr<IMainJob>> initTasks;
	std::vector<std::shared_ptr<IMainJob>> releaseTasks;

	static std::vector<spring::thread> workThreads;
	static int maxWorkThreads;
	static std::atomic<bool> workerRunning;
	static unsigned int counterInstance;

	static void WorkerThread(int num);
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_SCHEDULER_H_
