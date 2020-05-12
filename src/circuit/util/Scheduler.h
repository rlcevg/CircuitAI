/*
 * Scheduler.h
 *
 *  Created on: Aug 28, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UTIL_SCHEDULER_H_
#define SRC_CIRCUIT_UTIL_SCHEDULER_H_

#include "util/MultiQueue.h"
#include "util/GameTask.h"
#include "util/Defines.h"

#include "System/Threading/SpringThreading.h"

#include <functional>
#include <memory>
#include <list>

namespace circuit {

class IPathQuery;

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
	using PathFunc = std::function<void (const std::shared_ptr<IPathQuery>& query, int threadNum)>;
	using PathedFunc = std::function<void (const std::shared_ptr<IPathQuery>& query)>;

	/*
	 * Add task at specified frame, or execute immediately at next frame
	 */
	void RunTaskAt(const std::shared_ptr<CGameTask>& task, int frame = 0) {
		onceTasks.push_back({task, frame});
	}

	/*
	 * Add task at frame relative to current frame
	 */
	void RunTaskAfter(const std::shared_ptr<CGameTask>& task, int frame = 0) {
		onceTasks.push_back({task, lastFrame + frame});
	}

	/*
	 * Add task at specified interval
	 */
	void RunTaskEvery(const std::shared_ptr<CGameTask>& task, int frameInterval = FRAMES_PER_SEC, int frameOffset = 0);

	/*
	 * Process queued tasks at specified frame
	 */
	void ProcessTasks(int frame);

	/*
	 * Run concurrent task, finalize on success at main thread
	 */
	void RunParallelTask(const std::shared_ptr<CGameTask>& task, const std::shared_ptr<CGameTask>& onComplete = nullptr);

	/*
	 * Run concurrent pathfinder, finalize on complete at main thread
	 */
	void RunPathTask(const std::shared_ptr<IPathQuery>& query, PathFunc&& task, PathedFunc&& onComplete = nullptr);

	/*
	 * Remove scheduled task from queue
	 */
	void RemoveTask(const std::shared_ptr<CGameTask>& task);

	/*
	 * Run task on init. Not affected by RemoveTask
	 */
	void RunOnInit(std::shared_ptr<CGameTask>&& task) {
		initTasks.push_back(task);
	}

	/*
	 * Run task on release. Not affected by RemoveTask
	 */
	void RunOnRelease(std::shared_ptr<CGameTask>&& task) {
		releaseTasks.push_back(task);
	}

	int GetMaxPathThreads() const { return maxPathThreads; }

private:
	std::weak_ptr<CScheduler> self;
	int lastFrame;
	bool isProcessing;  // regular CGameTask

	bool isWorkProcess;  // parallel CGameTask
	int numPathProcess;  // parallel PathFunc
	Barrier barrier;
	std::atomic<bool> isRunning;  // parallel

	struct BaseContainer {
		BaseContainer(const std::shared_ptr<CGameTask>& task) : task(task) {}
		std::shared_ptr<CGameTask> task;
		bool operator==(const BaseContainer& other) const {
			return task == other.task;
		}
	};
	struct OnceTask: public BaseContainer {
		OnceTask(const std::shared_ptr<CGameTask>& task, int frame)
			: BaseContainer(task), frame(frame) {}
		int frame;
	};
	std::list<OnceTask> onceTasks;

	struct RepeatTask: public BaseContainer {
		RepeatTask(const std::shared_ptr<CGameTask>& task, int frameInterval, int lastFrame)
			: BaseContainer(task), frameInterval(frameInterval), lastFrame(lastFrame) {}
		int frameInterval;
		int lastFrame;
	};
	std::list<RepeatTask> repeatTasks;

	std::vector<std::shared_ptr<CGameTask>> removeTasks;

	struct WorkTask: public BaseContainer {
		WorkTask(const std::weak_ptr<CScheduler>& scheduler, const std::shared_ptr<CGameTask>& task,
				 const std::shared_ptr<CGameTask>& onComplete)
			: BaseContainer(task), onComplete(onComplete), scheduler(scheduler) {}
		std::shared_ptr<CGameTask> onComplete;
		std::weak_ptr<CScheduler> scheduler;
	};
	static CMultiQueue<WorkTask> workTasks;

	struct FinishTask: public BaseContainer {
		FinishTask(const std::shared_ptr<CGameTask>& task)
			: BaseContainer(task) {}
		FinishTask(const WorkTask& workTask)
			: BaseContainer(workTask.onComplete) {}
	};
	CMultiQueue<FinishTask> finishTasks;  // onComplete

	struct PathTask {
		PathTask(const std::weak_ptr<CScheduler>& scheduler, const std::shared_ptr<IPathQuery>& query,
				PathFunc&& task, PathedFunc&& onComplete)
			: scheduler(scheduler), query(query), task(std::move(task)), onComplete(std::move(onComplete)) {}
		std::weak_ptr<CScheduler> scheduler;
		std::weak_ptr<IPathQuery> query;
		PathFunc task;
		PathedFunc onComplete;
	};
	static CMultiQueue<PathTask> pathTasks;

	struct PathedTask {
		PathedTask(PathedFunc&& func)
			: onComplete(std::move(func)) {}
		PathedTask(const PathTask& pathTask)
			: query(pathTask.query), onComplete(std::move(pathTask.onComplete)) {}
		std::weak_ptr<IPathQuery> query;
		PathedFunc onComplete;
	};
	CMultiQueue<PathedTask> pathedTasks;  // onComplete

	std::vector<std::shared_ptr<CGameTask>> initTasks;
	std::vector<std::shared_ptr<CGameTask>> releaseTasks;

	static spring::thread workerThread;
	static std::vector<spring::thread> patherThreads;
	static int maxPathThreads;
	static std::atomic<bool> workerRunning;
	static unsigned int counterInstance;

	static void WorkerThread();
	static void PatherThread(int num);
};

} // namespace circuit

#endif // SRC_CIRCUIT_UTIL_SCHEDULER_H_
